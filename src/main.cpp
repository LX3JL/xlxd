//
//  main.cpp
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
#include "creflector.h"
#include "cudpsocket.h"
#include "cconfig.h"
#include "ctranscoder.h"

#include "csyslog.h"
#include <sys/stat.h>


////////////////////////////////////////////////////////////////////////////////////////
// global objects

CReflector  g_Reflector;

////////////////////////////////////////////////////////////////////////////////////////
// function declaration

#include "cusers.h"

// pid file, read by the dashboard and by systemd
static void WritePidFile(pid_t pid)
{
    const char *path = g_Config.GetPath("PID_PATH");
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if ( file.is_open() )
    {
        file << pid << std::endl;
    }
    else
    {
        std::cout << "Cannot write pid file " << path << std::endl;
    }
}

int main(int argc, const char * argv[])
{
#ifdef RUN_AS_DAEMON
    
    // redirect cout, cerr and clog to syslog
    syslogns::redirect cout_redir(std::cout);
    syslogns::redirect cerr_redir(std::cerr);
    syslogns::redirect clog_redir(std::clog);
#endif

    // check arguments, the callsign and IPs used to be arguments and are now in xlxd.ini
    if ( argc != 2 )
    {
        std::cout << "Usage: xlxd xlxd.ini" << std::endl;
        std::cout << "example: xlxd /xlxd/xlxd.ini" << std::endl;
        std::cout << "The callsign, listen IP and transcoder IP are now set in xlxd.ini" << std::endl;
        return 1;
    }

    // load the settings
    if ( !g_Config.Load(argv[1]) )
    {
        std::cout << "Error loading " << argv[1] << ", xlxd not started" << std::endl;
        return 1;
    }

#ifdef RUN_AS_DAEMON

    //Fork the Parent Process
    pid_t pid, sid;
    pid = ::fork();
    if ( pid < 0 )
    {
        exit(EXIT_FAILURE);
    }

    // We got a good pid, record it and Close the Parent Process
    // (writing it here means it exists before a service manager looks for it)
    if (pid > 0)
    {
        WritePidFile(pid);
        exit(EXIT_SUCCESS);
    }
    
    // Change File Mask
    ::umask(0);
    
    //Create a new Signature Id for our child
    sid = ::setsid();
    if (sid < 0)
    {
        exit(EXIT_FAILURE);
    }
    
    // Change Directory
    // If we cant find the directory we exit with failure.
    if ( (::chdir("/")) < 0)
    {
        exit(EXIT_FAILURE);
    }
    
    // Close Standard File Descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

#else
    WritePidFile(::getpid());
#endif

    // splash
    std::cout << "Starting xlxd " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_REVISION << std::endl << std::endl;

    // initialize reflector
    g_Reflector.SetCallsign(g_Config.GetCallsign());
    g_Reflector.SetListenIp(CIp(g_Config.GetListenIp()));
    g_Transcoder.SetServer(g_Config.GetTranscoderHost());
  
    // and let it run
    if ( !g_Reflector.Start() )
    {
        std::cout << "Error starting reflector" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "Reflector " << g_Reflector.GetCallsign()
              << "started and listening on " << g_Reflector.GetListenIp() << std::endl;

    // Log DSCP status once at startup
    CUdpSocket::LogDscpStatus();

#ifdef RUN_AS_DAEMON
	// run forever, picking up xlxd.ini changes
    while ( true )
    {
        CTimePoint::TaskSleepFor(CONFIG_RELOAD_PERIOD * 1000);
        g_Config.Reload();
        g_Transcoder.SetServer(g_Config.GetTranscoderHost());
    }
#else
    // wait any key
    for (;;)
    {
        CTimePoint::TaskSleepFor(CONFIG_RELOAD_PERIOD * 1000);
        g_Config.Reload();
        g_Transcoder.SetServer(g_Config.GetTranscoderHost());
#ifdef DEBUG_DUMPFILE
        g_Reflector.m_DebugFile.close();
#endif
    }
#endif
    
    // and wait for end
    g_Reflector.Stop();
    std::cout << "Reflector stopped" << std::endl;
    
    // done
    exit(EXIT_SUCCESS);
}
