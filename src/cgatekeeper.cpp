//
//  cgatekeeper.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 07/11/2015.
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
#include "ctimepoint.h"
#include "cgatekeeper.h"

////////////////////////////////////////////////////////////////////////////////////////

CGateKeeper g_GateKeeper;


////////////////////////////////////////////////////////////////////////////////////////
// constructor

CGateKeeper::CGateKeeper()
{
    m_bStopThread = false;
    m_pThread = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CGateKeeper::~CGateKeeper()
{
    // kill threads
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
    }
}


////////////////////////////////////////////////////////////////////////////////////////
// init & clode

bool CGateKeeper::Init(void)
{
    
    // load lists from files
    m_NodeWhiteList.LoadFromFile(WHITELIST_PATH);
    m_NodeBlackList.LoadFromFile(BLACKLIST_PATH);
    m_PeerList.LoadFromFile(INTERLINKLIST_PATH);
    
    // reset stop flag
    m_bStopThread = false;
    
    // start  thread;
    m_pThread = new std::thread(CGateKeeper::Thread, this);

    return true;
}

void CGateKeeper::Close(void)
{
    m_bStopThread = true;
    if ( m_pThread != NULL )
    {
        m_pThread->join();
        delete m_pThread;
        m_pThread = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// authorisations

bool CGateKeeper::MayLink(const CCallsign &callsign, const CIp &ip, int protocol, char *modules)
{
    bool ok = true;
    
    switch (protocol)
    {
        // repeaters
        case PROTOCOL_DEXTRA:
        case PROTOCOL_DPLUS:
        case PROTOCOL_DCS:
        case PROTOCOL_DMRPLUS:
        case PROTOCOL_DMRMMDVM:
        case PROTOCOL_YSF:
        case PROTOCOL_G3:
        case PROTOCOL_IMRS:
        case PROTOCOL_M17:
            // check IP & callsign listed OK
            ok &= IsNodeListedOk(callsign, ip);
            // todo: then apply any protocol specific authorisation for the operation
            break;
            
        // XLX interlinks
        case PROTOCOL_XLX:
            ok &= IsPeerListedOk(callsign, ip, modules);
            break;

        // YSF/NXDN/P25 peer links (outbound only, no gatekeeper check needed)
        case PROTOCOL_NXDN:
        case PROTOCOL_P25:
            // These peers are outbound connections initiated by us
            // based on interlink file, so always allow
            ok = true;
            break;

        // unsupported
        case PROTOCOL_NONE:
        default:
            ok = false;
            break;
    }
    
    // check loop detection link block
    if ( ok )
    {
        std::lock_guard<std::mutex> loopLock(m_LoopMutex);
        auto it = m_LoopPenalties.find(callsign);
        if ( it != m_LoopPenalties.end() && it->second.m_bLinkBlocked )
        {
            if ( it->second.m_BlockUntil.DurationSinceNow() < 0 )
            {
                // still blocked
                ok = false;
                std::cout << "Gatekeeper blocking linking of " << callsign << " @ " << ip
                          << " - loop detection penalty in effect" << std::endl;
            }
            else
            {
                // block expired, clear it
                it->second.m_bLinkBlocked = false;
            }
        }
    }

    // report
    if ( !ok )
    {
        std::cout << "Gatekeeper blocking linking of " << callsign << " @ " << ip << " using protocol " << protocol << std::endl;
    }

    // done
    return ok;
}
    
bool CGateKeeper::MayTransmit(const CCallsign &callsign, const CIp &ip, int protocol, char module)
{
    bool ok = true;
    
    switch (protocol)
    {
        // repeaters, protocol specific
        case PROTOCOL_ANY:
        case PROTOCOL_DEXTRA:
        case PROTOCOL_DPLUS:
        case PROTOCOL_DCS:
        case PROTOCOL_DMRPLUS:
        case PROTOCOL_DMRMMDVM:
        case PROTOCOL_YSF:
        case PROTOCOL_G3:
        case PROTOCOL_IMRS:
        case PROTOCOL_M17:
        case PROTOCOL_NXDN:
            // first check is IP & callsigned listed OK
            ok &= IsNodeListedOk(callsign, ip, module);
            // todo: then apply any protocol specific authorisation for the operation
            break;

        // XLX interlinks
        case PROTOCOL_XLX:
            ok &= IsPeerListedOk(callsign, ip, module);
            break;

        // P25 peer connections (peer-only mode, outbound connections we initiate)
        case PROTOCOL_P25:
            // P25 peers are outbound connections initiated by us
            // based on interlink file, so always allow
            ok = true;
            break;
            
        // unsupported
        case PROTOCOL_NONE:
        default:
            ok = false;
            break;
    }
    
    // check loop detection
    bool loopBlocked = false;
    if ( ok && module != ' ' )
    {
        if ( IsLoopSuppressed(callsign, module) )
        {
            ok = false;
            loopBlocked = true;
        }
    }

    // report (skip for loop blocks — already logged at strike time)
    if ( !ok && !loopBlocked )
    {
        std::cout << "Gatekeeper blocking transmitting of " << callsign << " @ " << ip << " using protocol " << protocol << std::endl;
    }

    // done
    return ok;
}

////////////////////////////////////////////////////////////////////////////////////////
// thread

void CGateKeeper::Thread(CGateKeeper *This)
{
    while ( !This->m_bStopThread )
    {
        // Wait 30 seconds
        CTimePoint::TaskSleepFor(30000);

        // have lists files changed ?
        if ( This->m_NodeWhiteList.NeedReload() )
        {
            This->m_NodeWhiteList.ReloadFromFile();
        }
        if ( This->m_NodeBlackList.NeedReload() )
        {
            This->m_NodeBlackList.ReloadFromFile();
        }
        if ( This->m_PeerList.NeedReload() )
        {
            This->m_PeerList.ReloadFromFile();
        }

        // purge stale loop detection entries
        {
            std::lock_guard<std::mutex> lock(This->m_LoopMutex);
            auto it = This->m_LoopPenalties.begin();
            while ( it != This->m_LoopPenalties.end() )
            {
                CLoopPenalty &p = it->second;
                bool blockExpired = (p.m_BlockUntil.DurationSinceNow() >= 0);
                bool strikesDecayed = (p.m_StrikeCount == 0) ||
                    (p.m_LastStrikeTime.DurationSinceNow() > LOOP_STRIKE_DECAY_SEC);
                if ( blockExpired && strikesDecayed && !p.m_bLinkBlocked )
                {
                    it = This->m_LoopPenalties.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
// operation helpers

bool CGateKeeper::IsNodeListedOk(const CCallsign &callsign, const CIp &ip, char module) const
{
    bool ok = true;
    bool whitelisted = false;

    // first check IP

    // next, check callsign
    if ( ok )
    {
        // first check if callsign is in white list
        // note if white list is empty, everybody is authorized
        const_cast<CCallsignList &>(m_NodeWhiteList).Lock();
        if ( !m_NodeWhiteList.empty() )
        {
            whitelisted = m_NodeWhiteList.IsCallsignListedWithWildcard(callsign, module);
            ok = whitelisted;
        }
        const_cast<CCallsignList &>(m_NodeWhiteList).Unlock();

        // then check if not blacklisted
        const_cast<CCallsignList &>(m_NodeBlackList).Lock();
        ok &= !m_NodeBlackList.IsCallsignListedWithWildcard(callsign);
        const_cast<CCallsignList &>(m_NodeBlackList).Unlock();

        // require at least one digit for valid amateur callsigns,
        // but skip for explicitly whitelisted callsigns and the
        // DVREF validator callsign (used to verify reflector status)
        if ( ok && !whitelisted && !callsign.HasSameCallsignWithWildcard(CCallsign("DVREFCK")) )
        {
            ok &= callsign.HasNumber();
        }
    }

    // done
    return ok;

}

bool CGateKeeper::IsPeerListedOk(const CCallsign &callsign, const CIp &ip, char module) const
{
    bool ok = true;
    
    // first check IP
    
    // next, check callsign
    if ( ok )
    {
        // look for an exact match in the list
        const_cast<CPeerCallsignList &>(m_PeerList).Lock();
        if ( !m_PeerList.empty() )
        {
            ok = m_PeerList.IsCallsignListed(callsign, module);
        }
        const_cast<CPeerCallsignList &>(m_PeerList).Unlock();
    }
    
    // done
    return ok;
}

bool CGateKeeper::IsPeerListedOk(const CCallsign &callsign, const CIp &ip, char *modules) const
{
    bool ok = true;

    // first check IP

    // next, check callsign
    if ( ok )
    {
        // look for an exact match in the list
        const_cast<CPeerCallsignList &>(m_PeerList).Lock();
        if ( !m_PeerList.empty() )
        {
            ok = m_PeerList.IsCallsignListed(callsign, modules);
        }
        const_cast<CPeerCallsignList &>(m_PeerList).Unlock();
    }

    // done
    return ok;
}

////////////////////////////////////////////////////////////////////////////////////////
// loop detection

bool CGateKeeper::ReportStreamClose(char module, const CCallsign &myCallsign, uint32 frameCount, bool isPeer)
{
    std::lock_guard<std::mutex> lock(m_LoopMutex);

    int moduleIdx = module - 'A';
    if ( moduleIdx < 0 || moduleIdx >= NB_OF_MODULES )
        return false;

    CStreamHistory &prev = m_StreamHistory[moduleIdx];
    bool shouldDisconnect = false;
    bool isShort = (frameCount < LOOP_DETECT_SHORT_FRAMES);

    // count consecutive short streams from same callsign on same module
    bool patternContinues = false;
    if ( prev.m_bValid && prev.m_MyCallsign.HasSameCallsign(myCallsign) && isShort )
    {
        double gap = prev.m_CloseTime.DurationSinceNow();

        // check if this callsign already has active strikes (not yet decayed)
        bool hasActiveStrikes = false;
        auto it = m_LoopPenalties.find(myCallsign);
        if ( it != m_LoopPenalties.end() && it->second.m_StrikeCount > 0 )
        {
            if ( it->second.m_LastStrikeTime.DurationSinceNow() > LOOP_STRIKE_DECAY_SEC )
            {
                it->second.m_StrikeCount = 0;
                it->second.m_bLinkBlocked = false;
            }
            else
            {
                hasActiveStrikes = true;
            }
        }

        if ( gap < LOOP_DETECT_GAP_SEC || hasActiveStrikes )
        {
            patternContinues = true;
        }
    }

    // update consecutive count
    if ( patternContinues )
    {
        prev.m_ConsecutiveShort++;
    }
    else
    {
        prev.m_ConsecutiveShort = isShort ? 1 : 0;
    }

    // only strike when enough consecutive short streams have been seen
    if ( patternContinues && prev.m_ConsecutiveShort >= LOOP_DETECT_SHORT_COUNT )
    {
        CLoopPenalty &penalty = m_LoopPenalties[myCallsign];

        // decay check (in case this is first access for this callsign)
        if ( penalty.m_StrikeCount > 0 && penalty.m_LastStrikeTime.DurationSinceNow() > LOOP_STRIKE_DECAY_SEC )
        {
            penalty.m_StrikeCount = 0;
            penalty.m_bLinkBlocked = false;
        }

        penalty.m_StrikeCount++;
        penalty.m_LastStrikeTime.Now();
        penalty.m_bBlockLogged = true;

        // reset count so next batch needs another full run
        prev.m_ConsecutiveShort = 0;

        if ( penalty.m_StrikeCount == 1 )
        {
            penalty.m_BlockUntil.SetFutureSeconds(LOOP_BACKOFF_1_SEC);
            std::cout << "Loop detection: " << myCallsign << " on module " << module
                      << " - strike 1, TX blocked for " << LOOP_BACKOFF_1_SEC << "s" << std::endl;
        }
        else if ( penalty.m_StrikeCount == 2 )
        {
            penalty.m_BlockUntil.SetFutureSeconds(LOOP_BACKOFF_2_SEC);
            std::cout << "Loop detection: " << myCallsign << " on module " << module
                      << " - strike 2, TX blocked for " << LOOP_BACKOFF_2_SEC << "s" << std::endl;
        }
        else
        {
            penalty.m_BlockUntil.SetFutureSeconds(LOOP_BACKOFF_3_SEC);
            std::cout << "Loop detection: " << myCallsign << " on module " << module
                      << " - strike 3, TX blocked for " << LOOP_BACKOFF_3_SEC << "s";

            if ( !isPeer )
            {
                penalty.m_bLinkBlocked = true;
                shouldDisconnect = true;
                std::cout << ", link blocked, disconnecting client";
            }
            std::cout << std::endl;
        }
    }

    // update history for this module
    prev.m_MyCallsign = myCallsign;
    prev.m_CloseTime.Now();
    prev.m_FrameCount = frameCount;
    prev.m_bValid = true;

    return shouldDisconnect;
}

bool CGateKeeper::IsCallsignLoopBlocked(const CCallsign &myCallsign, const char *peerDescr)
{
    // Pure-read of m_LoopPenalties: is there an active block window for
    // this callsign right now? Used by the peer-traffic header gate
    // where MayTransmit is bypassed by design.
    //
    // Distinct from IsLoopSuppressed: this method does NOT touch
    // m_bBlockLogged or emit a "TX block expired" log. Those concerns
    // belong to the MayTransmit path. Here we only emit a rate-limited
    // "dropping incoming header" log so the operator can see which
    // user callsign was dropped via which interlink.
    std::lock_guard<std::mutex> lock(m_LoopMutex);

    auto it = m_LoopPenalties.find(myCallsign);
    if ( it == m_LoopPenalties.end() )
    {
        return false;
    }

    // DurationSinceNow returns (now - m_BlockUntil). Negative ⇒ block is
    // in the future (still active). Positive ⇒ block already expired.
    if ( it->second.m_BlockUntil.DurationSinceNow() >= 0 )
    {
        return false;
    }

    // Block is active. Rate-limit the drop log so a sustained loop
    // doesn't drown the system journal. CTimePoint default-constructs
    // to now(), not the epoch — so the m_bDropEverLogged bool gates
    // the first emission; subsequent emissions gate on the elapsed
    // time since the last log.
    bool shouldLog = !it->second.m_bDropEverLogged ||
                     it->second.m_LastDropLogged.DurationSinceNow() >= LOOP_DROP_LOG_RATE_LIMIT_SEC;
    if ( shouldLog )
    {
        double secsLeft = -it->second.m_BlockUntil.DurationSinceNow();
        std::cout << "Loop block: dropping incoming header from "
                  << myCallsign << " (block expires in "
                  << (int)secsLeft << "s";
        if ( peerDescr != NULL )
        {
            std::cout << " via " << peerDescr;
        }
        std::cout << ")" << std::endl;
        it->second.m_LastDropLogged.Now();
        it->second.m_bDropEverLogged = true;
    }

    return true;
}

bool CGateKeeper::IsLoopSuppressed(const CCallsign &myCallsign, char module)
{
    std::lock_guard<std::mutex> lock(m_LoopMutex);

    auto it = m_LoopPenalties.find(myCallsign);
    if ( it != m_LoopPenalties.end() )
    {
        if ( it->second.m_BlockUntil.DurationSinceNow() < 0 )
        {
            return true;
        }
        else if ( it->second.m_bBlockLogged )
        {
            std::cout << "Loop detection: TX block expired for " << myCallsign
                      << " on module " << module
                      << " (strike " << it->second.m_StrikeCount << ")" << std::endl;
            it->second.m_bBlockLogged = false;
        }
    }

    return false;
}

