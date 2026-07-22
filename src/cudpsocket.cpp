//
//  cudpsocket.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 31/10/2015.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//
// ----------------------------------------------------------------------------
//    This file is part of xlxd.
//
//    xlxd is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    xlxd is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Foobar.  If not, see <http://www.gnu.org/licenses/>. 
// ----------------------------------------------------------------------------

#include "main.h"
#include <string.h>
#include "creflector.h"
#include "cudpsocket.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CUdpSocket::CUdpSocket()
{
    m_Socket = -1;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CUdpSocket::~CUdpSocket()
{
    if ( m_Socket != -1 )
    {
        Close();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// open & close

bool CUdpSocket::Open(uint16 uiPort)
{
    bool open = false;

    // create socket
    m_Socket = socket(PF_INET,SOCK_DGRAM,0);
    if ( m_Socket != -1 )
    {
        // initialize sockaddr struct
        ::memset(&m_SocketAddr, 0, sizeof(struct sockaddr_in));
        m_SocketAddr.sin_family = AF_INET;
        m_SocketAddr.sin_port = htons(uiPort);
        m_SocketAddr.sin_addr.s_addr = inet_addr(g_Reflector.GetListenIp());

        if ( bind(m_Socket, (struct sockaddr *)&m_SocketAddr, sizeof(struct sockaddr_in)) == 0 )
        {
            fcntl(m_Socket, F_SETFL, O_NONBLOCK);

            // IP_TOS is now set per-class in Send / SendVoice via
            // SetTosIfChanged(), not once at socket open. The prior
            // blanket setsockopt here marked EVERY outgoing packet with
            // the voice DSCP, including keepalives and logins, which
            // defeated the purpose of QoS signalling.
            //
            // Reset the cached TOS state here too, in case a previously-
            // closed socket is being re-opened in the same CUdpSocket.
            // A fresh kernel socket has IP_TOS=0 and m_iCurrentTos=0 +
            // m_bTosValid=true matches that state, so the first
            // SetTosIfChanged(0) after open is a no-op cache hit.
            m_iCurrentTos = 0;
            m_bTosValid   = true;

            open = true;
        }
        else
        {
            close(m_Socket);
            m_Socket = -1;
        }
    }

    // done
    return open;
}

void CUdpSocket::Close(void)
{
    if ( m_Socket != -1 )
    {
        close(m_Socket);
        m_Socket = -1;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// read

int CUdpSocket::Receive(CBuffer *Buffer, CIp *Ip, int timeout)
{
    struct sockaddr_in Sin;
    fd_set FdSet;
    unsigned int uiFromLen = sizeof(struct sockaddr_in);
    int iRecvLen = -1;
    struct timeval tv;
    
    // socket valid ?
    if ( m_Socket != -1 )
    {
        // control socket
        FD_ZERO(&FdSet);
        FD_SET(m_Socket, &FdSet);
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        select(m_Socket + 1, &FdSet, 0, 0, &tv);
        
        // allocate buffer
        Buffer->resize(UDP_BUFFER_LENMAX);
        
        // read
        iRecvLen = (int)recvfrom(m_Socket,
            (void *)Buffer->data(), UDP_BUFFER_LENMAX,
            0, (struct sockaddr *)&Sin, &uiFromLen);
        
        // handle
        if ( iRecvLen != -1 )
        {
            // adjust buffer size
            Buffer->resize(iRecvLen);
            
            // get IP
            Ip->SetSockAddr(&Sin);
        }
    }
 
    // done
    return iRecvLen;
}

////////////////////////////////////////////////////////////////////////////////////////
// TOS class switching — applied via per-class setsockopt with caching so
// bursts of same-class sends pay the syscall only once per class switch.
// IPv4 uses IP_TOS; a future IPv6 port would need IPV6_TCLASS alongside.
//
// Always called — even when DSCP_MARKING_ENABLE is 0. In the disabled build
// SendVoice() is an inline alias for Send() (see cudpsocket.h), so every
// send path routes through here with tos=0. m_iCurrentTos starts at 0 and
// matches the kernel default for a fresh socket, so the first call is a
// cache hit; every subsequent Send() is also a cache hit. Result in the
// "DSCP disabled" build: zero setsockopt syscalls — not one per Send —
// so the runtime cost of this indirection is nil.

// One-shot failure log for SetTosIfChanged. Shared across all sockets:
// a kernel that rejects an IP_TOS value will reject it for every socket,
// so a single log line per process is enough to alert the operator.
// Declared outside the DSCP_MARKING_ENABLE guard because SetTosIfChanged
// is also called from Send() with tos=0 when DSCP marking is disabled.
static std::atomic<bool> g_bTosErrorLogged(false);

void CUdpSocket::SetTosIfChanged(int tos)
{
    if ( m_Socket == -1 ) return;
    // Cache hit is valid only when we've confirmed the kernel actually
    // accepted the value (m_bTosValid). After a setsockopt failure the
    // cache doesn't reflect reality, and m_bTosValid being false forces
    // a retry regardless of what `tos` we're called with.
    if ( m_bTosValid && tos == m_iCurrentTos ) return;
    // IPv6: use IPPROTO_IPV6 / IPV6_TCLASS on the v6 socket.
    if ( setsockopt(m_Socket, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) == 0 )
    {
        m_iCurrentTos = tos;
        m_bTosValid   = true;
        return;
    }

    // setsockopt failed. The kernel's IP_TOS is unchanged — still whatever
    // the previous successful call (or the default 0) set it to. Mark the
    // cache invalid so the next call — with any `tos` value — misses the
    // cache check and retries the syscall. Do NOT encode the failure
    // state in m_iCurrentTos via a negative sentinel: Linux historically
    // masks the `int` argument to `u8` inside ip_setsockopt, so a value
    // of -1 becomes 0xFF (a legal TOS byte) and the retry would silently
    // succeed on that second call, leaving the wrong TOS on the wire
    // with no further diagnostic.
    int saved_errno = errno;
    m_bTosValid = false;

    // Log once per process on first failure. Common causes: bad socket
    // state (EBADF), kernel seccomp/SELinux policy rejecting IP_TOS
    // (EPERM), or an invalid value (EINVAL) — the last only possible
    // if DSCP_VALUE was bypassed by the main.h static_assert somehow
    // (e.g. direct call with out-of-range `tos`).
    //
    // strerror() is not thread-safe on all platforms, but the exchange()
    // gate ensures at most one thread reaches this branch on first
    // failure. A garbled first-failure log line is the worst outcome;
    // strerror_r() would eliminate the theoretical race if this ever
    // becomes a concern. All subsequent failures skip the log entirely.
    if ( !g_bTosErrorLogged.exchange(true) )
    {
        std::cout << "Warning: setsockopt(IP_TOS=" << tos
                  << ") failed: errno=" << saved_errno
                  << " (" << ::strerror(saved_errno) << ") — "
                  << "QoS marking may be wrong on this kernel; "
                  << "subsequent calls will retry per-send" << std::endl;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// write — signalling (keepalives, logins, acks, disconnects).
// DSCP 0 / best-effort so the voice class gets scheduled ahead of these
// on any hop that respects DSCP.

int CUdpSocket::Send(const CBuffer &Buffer, const CIp &Ip)
{
    if ( m_Socket == -1 ) return -1;
    SetTosIfChanged(0);
    CIp temp(Ip);
    return (int)::sendto(m_Socket,
           (void *)Buffer.data(), Buffer.size(),
           0, (struct sockaddr *)temp.GetSockAddr(), sizeof(struct sockaddr_in));
}

int CUdpSocket::Send(const char *Buffer, const CIp &Ip)
{
    if ( m_Socket == -1 ) return -1;
    SetTosIfChanged(0);
    CIp temp(Ip);
    return (int)::sendto(m_Socket,
           (void *)Buffer, ::strlen(Buffer),
           0, (struct sockaddr *)temp.GetSockAddr(), sizeof(struct sockaddr_in));
}

int CUdpSocket::Send(const CBuffer &Buffer, const CIp &Ip, uint16 destport)
{
    if ( m_Socket == -1 ) return -1;
    SetTosIfChanged(0);
    CIp temp(Ip);
    temp.GetSockAddr()->sin_port = htons(destport);
    return (int)::sendto(m_Socket,
                         (void *)Buffer.data(), Buffer.size(),
                         0, (struct sockaddr *)temp.GetSockAddr(), sizeof(struct sockaddr_in));
}

int CUdpSocket::Send(const char *Buffer, const CIp &Ip, uint16 destport)
{
    if ( m_Socket == -1 ) return -1;
    SetTosIfChanged(0);
    CIp temp(Ip);
    temp.GetSockAddr()->sin_port = htons(destport);
    return (int)::sendto(m_Socket,
                         (void *)Buffer, ::strlen(Buffer),
                         0, (struct sockaddr *)temp.GetSockAddr(), sizeof(struct sockaddr_in));
}

////////////////////////////////////////////////////////////////////////////////////////
// write - voice packets with DSCP marking

#if (DSCP_MARKING_ENABLE == 1)
// Single global flag for DSCP logging - shared across all SendVoice calls
static std::atomic<bool> g_bDscpLogged(false);

int CUdpSocket::SendVoice(const CBuffer &Buffer, const CIp &Ip)
{
    if ( m_Socket == -1 ) return -1;
    SetTosIfChanged(DSCP_VALUE << 2);   // voice class — marked DSCP_VALUE

    CIp temp(Ip);
    int result;
    int retries = 0;
    do
    {
        result = (int)::sendto(m_Socket,
               (void *)Buffer.data(), Buffer.size(),
               0, (struct sockaddr *)temp.GetSockAddr(), sizeof(struct sockaddr_in));
        int saved_errno = errno;
        if ( result == -1 && saved_errno == EAGAIN && retries < 5 )
        {
            usleep(1000);  // 1ms backoff to let kernel flush send buffer
            // TOS persists across retries — no need to re-call SetTosIfChanged.
            retries++;
        }
        else
        {
            break;
        }
    } while ( true );

    return result;
}

int CUdpSocket::SendVoice(const CBuffer &Buffer, const CIp &Ip, uint16 destport)
{
    if ( m_Socket == -1 ) return -1;
    SetTosIfChanged(DSCP_VALUE << 2);   // voice class — marked DSCP_VALUE

    CIp temp(Ip);
    temp.GetSockAddr()->sin_port = htons(destport);
    int result;
    int retries = 0;
    do
    {
        result = (int)::sendto(m_Socket,
                         (void *)Buffer.data(), Buffer.size(),
                         0, (struct sockaddr *)temp.GetSockAddr(), sizeof(struct sockaddr_in));
        int saved_errno = errno;
        if ( result == -1 && saved_errno == EAGAIN && retries < 5 )
        {
            usleep(1000);
            retries++;
        }
        else
        {
            break;
        }
    } while ( true );

    return result;
}

void CUdpSocket::LogDscpStatus(void)
{
    if ( !g_bDscpLogged )
    {
        // DSCP_VALUE range is enforced at compile time by static_assert in
        // main.h, so the invalid-range branch is unreachable in a correctly
        // built binary. Kept for defence-in-depth against NDEBUG/asserts-
        // disabled builds or a future refactor.
        if ( DSCP_VALUE >= 0 && DSCP_VALUE <= 63 )
        {
            std::cout << "Voice packets marked DSCP " << DSCP_VALUE
                      << " (EF); signalling packets unmarked (BE)" << std::endl;
        }
        else
        {
            std::cout << "Error: Invalid DSCP_VALUE " << DSCP_VALUE
                      << " (must be 0-63), QoS disabled" << std::endl;
        }
        g_bDscpLogged = true;
    }
}
#endif


