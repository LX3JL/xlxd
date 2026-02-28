find_package(FTD2XX)

add_executable(ambed
        cambepacket.cpp cambepacket.h
        cambeserver.cpp cambeserver.h
        cbuffer.cpp cbuffer.h
        ccallsign.cpp ccallsign.h
        ccontroller.cpp ccontroller.h
        cftdidevicedescr.cpp cftdidevicedescr.h
        cip.cpp cip.h
        cpacket.cpp cpacket.h
        cpacketqueue.cpp cpacketqueue.h
        cstream.cpp cstream.h
        ctimepoint.cpp ctimepoint.h
        cudpsocket.cpp cudpsocket.h
        cusb3000interface.cpp cusb3000interface.h
        cusb3003df2etinterface.cpp cusb3003df2etinterface.h
        cusb3003hrinterface.cpp cusb3003hrinterface.h
        cusb3003interface.cpp cusb3003interface.h
        cusb3xxxinterface.cpp cusb3xxxinterface.h
        cvocodecchannel.cpp cvocodecchannel.h
        cvocodecinterface.cpp cvocodecinterface.h
        cvocodecs.cpp cvocodecs.h
        cvoicepacket.cpp cvoicepacket.h
        main.cpp main.h
        syslog.h)

target_link_libraries(ambed
        LINK_PUBLIC
        Threads::Threads
        ftd2xx)

target_compile_options(ambed
        PRIVATE
        -Wall)
