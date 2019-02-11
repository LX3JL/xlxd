//
//  main.cpp
//  ambedtest
//
//  Created by Jean-Luc Deltombe (LX3JL) on 12/05/2017.
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
#include "ctranscoder.h"

#define NB_STREAM       1

int main(int argc, const char * argv[])
{
    std::vector<CCodecStream *> Streams;
    
    // check args
    if ( argc != 6 )
    {
        std::cout << "Usage: ambedtest myip ambedip nbdmrstreams nbdstarstreams nbcodec2streams" << std::endl;
        std::cout << "example: ambed 192.168.178.212 127.0.0.1 2 2 2" << std::endl;
        return 1;
    }
    
    // init the transcoder
    std::cout << "Connecting to ambed server " << std::endl;
    g_Transcoder.Init(CIp(argv[1]), CIp(argv[2]));
    while ( !g_Transcoder.IsAmbedConnected() );
    std::cout << "Press enter to start test" << std::endl;
    std::cin.get();

    // create streams
    int nDmr = atoi(argv[3]);
    int nDstar = atoi(argv[4]);
    int nCodec2 = atoi(argv[5]);
    
    for ( int i = 0; i < nDmr; i++ )
    {
        CTimePoint::TaskSleepFor(300);
        Streams.push_back(g_Transcoder.GetStream(CODEC_AMBE2PLUS));
    }
    
    for ( int i = 0; i < nDstar; i++ )
    {
        CTimePoint::TaskSleepFor(300);
        Streams.push_back(g_Transcoder.GetStream(CODEC_AMBEPLUS));
    }

    for ( int i = 0; i < nCodec2; i++ )
    {
        CTimePoint::TaskSleepFor(300);
        Streams.push_back(g_Transcoder.GetStream(CODEC_CODEC2));
    }
    
    // and loop wait
    std::cin.get();
    
    // close
    for ( int i = 0; i < Streams.size(); i++ )
    {
        g_Transcoder.ReleaseStream(Streams[i]);
    }
    g_Transcoder.Close();
    
    // done
    return 0;
}
