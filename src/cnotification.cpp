//
//  cnotification.cpp
//  xlxd
//
//  Created by Jean-Luc on 05/12/2015.
//  Copyright © 2015 Jean-Luc. All rights reserved.
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
#include "cnotification.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CNotification::CNotification()
{
    // init variables
    m_iId = NOTIFICATION_NONE;
}

CNotification::CNotification(const CNotification &Notification)
{
    m_iId = Notification.m_iId;
    m_Callsign = Notification.m_Callsign;
}

CNotification::CNotification(int iId)
{
    m_iId = iId;
}

CNotification::CNotification(int iId, const CCallsign &Callsign)
{
    m_iId = iId;
    m_Callsign = Callsign;
}
