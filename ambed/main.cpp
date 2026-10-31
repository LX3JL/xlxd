//
//  main.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 13/04/2017.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//
// ----------------------------------------------------------------------------
//    This file is part of ambed.
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
#include "cambeserver.h"
#include "cudpsocket.h"
#include "cconfig.h"

#include "syslog.h"
#include <sys/stat.h>

////////////////////////////////////////////////////////////////////////////////////////
// global objects


////////////////////////////////////////////////////////////////////////////////////////
// function declaration


int main(int argc, const char * argv[])
{
#ifdef RUN_AS_DAEMON
    
    // redirect cout, cerr and clog to syslog
    syslog::redirect cout_redir(std::cout);
    syslog::redirect cerr_redir(std::cerr);
    syslog::redirect clog_redir(std::clog);
    
    //Fork the Parent Process
    pid_t pid, sid;
    pid = ::fork();
    //pid = ::vfork();
    if ( pid < 0 )
    {
        exit(EXIT_FAILURE);
    }
    
    // We got a good pid, Close the Parent Process
    if (pid > 0)
    {
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
    
#endif
    
    // check arguments, the listen IP used to be an argument and is now in ambed.ini
    if ( argc > 2 || (argc == 2 && g_Config.IsValidIp(argv[1])) )
    {
        std::cout << "Usage: ambed [ambed.ini]" << std::endl;
        std::cout << "example: ambed /ambed/ambed.ini" << std::endl;
        std::cout << "The listen IP is now LISTEN_IP in ambed.ini" << std::endl;
        return 1;
    }

    // load the settings, main.h defaults without an ambed.ini
    if ( argc == 2 )
    {
        g_Config.Load(argv[1]);
    }

    // initialize ambeserver
    g_AmbeServer.SetListenIp(CIp(g_Config.GetListenIp().c_str()));

    // and let it run
    std::cout << "Starting AMBEd " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_REVISION << std::endl << std::endl;
    if ( !g_AmbeServer.Start() )
    {
        std::cout << "Error starting AMBEd" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "AMBEd started and listening on " << g_AmbeServer.GetListenIp() << ":" << g_Config.GetListenPort() << std::endl;

    // Log DSCP status once at startup
    CUdpSocket::LogDscpStatus();

    // run forever — shutdown is handled by process termination
    // (destructors of static objects run on exit)
    for (;;)
    {
        CTimePoint::TaskSleepFor(CONFIG_RELOAD_PERIOD * 1000);
        g_Config.Reload();
    }
}
