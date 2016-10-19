//
//  cpacket.h
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 01/11/2015.
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

#ifndef cpacket_h
#define cpacket_h

////////////////////////////////////////////////////////////////////////////////////////

// Origin Id

#define ORIGIN_LOCAL    0
#define ORIGIN_PEER     1

////////////////////////////////////////////////////////////////////////////////////////
// class

class CClient;

class CPacket
{
public:
    // constructor
    CPacket();
    CPacket(const CClient *, uint16 sid, uint8 pid);
    
    // destructor
    virtual ~CPacket() {};
    
    // virtual duplication
    virtual CPacket *Duplicate(void) const;
    
    // identity
    virtual bool IsDvHeader(void) const           { return false; }
    virtual bool IsDvFrame(void) const            { return false; }
    virtual bool IsLastPacket(void) const         { return false; }
    
    // get
    virtual bool IsValid(void) const              { return true; }
    uint16 GetStreamId(void) const                { return m_uiStreamId; }
    uint8  GetPacketId(void) const                { return m_uiPacketId; }
    uint8  GetModuleId(void) const                { return m_uiModuleId; }
    const CClient * GetOriginClient(void) const   { return m_OriginClient; }
    bool   IsLocalOrigin(void) const              { return (m_uiOriginId == ORIGIN_LOCAL); }
    
    // set
    void SetModuleId(uint8 uiId)                  { m_uiModuleId = uiId; }
    void SetLocalOrigin(void)                     { m_uiOriginId = ORIGIN_LOCAL; }
    void SetRemotePeerOrigin(void)                { m_uiOriginId = ORIGIN_PEER; }

protected:
    // data
    uint16  m_uiStreamId;
    uint8   m_uiPacketId;
    uint8   m_uiModuleId;
    uint8   m_uiOriginId;
    const CClient * m_OriginClient;
};



////////////////////////////////////////////////////////////////////////////////////////
#endif /* cpacket_h */
