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

#include "syslog.h"
#include <csignal>
#include <sys/stat.h>


////////////////////////////////////////////////////////////////////////////////////////
// global objects

CReflector  g_Reflector;

////////////////////////////////////////////////////////////////////////////////////////
// function declaration

#include "cusers.h"

// Returns caught termination signal or -1 on error
static int wait_for_termination()
{
    sigset_t waitset;
    siginfo_t siginfo;

    sigemptyset(&waitset);
    sigaddset(&waitset, SIGTERM);
    sigaddset(&waitset, SIGINT);
    sigaddset(&waitset, SIGQUIT);
    sigaddset(&waitset, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &waitset, nullptr);

    // Now wait for termination signal
    int result = -1;
    while (result < 0)
    {
        result = sigwaitinfo(&waitset, &siginfo);
        if (result == -1 && errno == EINTR)
        {
            // try again
            if (errno == EINTR)
                continue;

            // an unexpected error occurred, consider it fatal
            break;
        }
    }
    return result;
}

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

    // check arguments
    if ( argc != 4 )
    {
        std::cout << "Usage: xlxd callsign xlxdip ambedip" << std::endl;
        std::cout << "example: xlxd XLX999 192.168.178.212 127.0.0.1" << std::endl;
        return 1;
    }

    // splash
    std::cout << "Starting xlxd " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_REVISION << std::endl << std::endl;

    // initialize reflector
    g_Reflector.SetCallsign(argv[1]);
    g_Reflector.SetListenIp(CIp(argv[2]));
    g_Reflector.SetTranscoderIp(CIp(CIp(argv[3])));

    // Block all signals while starting up the reflector -- we don't
    // want any of the worker threads handling them.
    sigset_t sigblockall, sigorig;
    sigfillset(&sigblockall);
    pthread_sigmask(SIG_SETMASK, &sigblockall, &sigorig);

    // and let it run
    if ( !g_Reflector.Start() )
    {
        std::cout << "Error starting reflector" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Restore main thread default signal state
    pthread_sigmask(SIG_SETMASK, &sigorig, nullptr);

    std::cout << "Reflector " << g_Reflector.GetCallsign()
              << "started and listening on " << g_Reflector.GetListenIp() << std::endl;
    
    // and wait for end
    wait_for_termination();

    g_Reflector->Stop();
    std::cout << "Reflector stopped" << std::endl;
    
    // done
    exit(EXIT_SUCCESS);
}
