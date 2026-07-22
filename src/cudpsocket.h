//
//  cudpsocket.h
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

#ifndef cudpsocket_h
#define cudpsocket_h

#include <sys/types.h>
//#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "main.h"
#include "cip.h"
#include "cbuffer.h"

////////////////////////////////////////////////////////////////////////////////////////
// define

#define UDP_BUFFER_LENMAX       1024


////////////////////////////////////////////////////////////////////////////////////////
// class
//
// Per-class DSCP marking:
//   Send()      → IP_TOS 0  (signalling: keepalives, logins, acks, disconnects)
//   SendVoice() → IP_TOS DSCP_VALUE<<2  (voice flow: voice frames, headers, EoT)
//
// IP_TOS is a per-socket kernel attribute set via setsockopt. To avoid the
// syscall on every packet, m_iCurrentTos caches the last value set; only a
// class switch (voice→signalling or vice-versa) pays the syscall.
//
// Thread-safety assumption: each CUdpSocket is accessed from a single thread
// (the owning protocol's Task thread). m_iCurrentTos is a plain int, safe
// under single-writer access only. A future RX/TX thread split that puts
// voice and signalling sends on different threads MUST add synchronisation
// here (std::atomic<int>, or a mutex, or a per-thread cached TOS).

class CUdpSocket
{
public:
    // constructor
    CUdpSocket();

    // destructor
    ~CUdpSocket();

    // open & close
    bool Open(uint16);
    void Close(void);
    int  GetSocket(void)        { return m_Socket; }

    // read
    int Receive(CBuffer *, CIp *, int);

    // write
    int Send(const CBuffer &, const CIp &);
    int Send(const CBuffer &, const CIp &, uint16);
    int Send(const char *, const CIp &);
    int Send(const char *, const CIp &, uint16);

    // write - voice packets with DSCP marking
#if (DSCP_MARKING_ENABLE == 1)
    int SendVoice(const CBuffer &, const CIp &);
    int SendVoice(const CBuffer &, const CIp &, uint16);
    static void LogDscpStatus(void);
#else
    // When DSCP disabled, SendVoice is just an alias for Send
    int SendVoice(const CBuffer &Buffer, const CIp &Ip) { return Send(Buffer, Ip); }
    int SendVoice(const CBuffer &Buffer, const CIp &Ip, uint16 port) { return Send(Buffer, Ip, port); }
    static void LogDscpStatus(void) {}
#endif

protected:
    // data
    int                 m_Socket;
    struct sockaddr_in  m_SocketAddr;
    int                 m_iCurrentTos = 0;  // last IP_TOS set on m_Socket.
                                            // Zero matches a fresh socket's
                                            // kernel default so the first
                                            // SetTosIfChanged(0) is a no-op.
    bool                m_bTosValid = true; // whether m_iCurrentTos reflects
                                            // the kernel's real state. Cleared
                                            // on setsockopt failure so the
                                            // next SetTosIfChanged call
                                            // retries the syscall regardless
                                            // of what `tos` it sees. Using a
                                            // separate flag (instead of a
                                            // negative sentinel in m_iCurrentTos)
                                            // avoids assumptions about what
                                            // integer values the kernel
                                            // rejects for IP_TOS — e.g. Linux
                                            // historically masks `int` to
                                            // `u8`, so -1 becomes 0xFF, a
                                            // legal TOS value that would
                                            // silently succeed on retry.

private:
    // Sets IP_TOS only when it differs from the cached value. Called by
    // Send (tos=0) and SendVoice (tos=DSCP_VALUE<<2) before each sendto.
    // A burst of voice frames or keepalives pays the setsockopt syscall
    // once, not once per frame. Single-threaded access only — see class
    // comment above.
    void SetTosIfChanged(int tos);
};

////////////////////////////////////////////////////////////////////////////////////////
#endif /* cudpsocket_h */
