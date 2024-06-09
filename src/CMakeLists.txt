add_executable(xlxd
        cbmclient.cpp cbmclient.h
        cbmpeer.cpp cbmpeer.h
        cbptc19696.cpp cbptc19696.h
        cbuffer.cpp cbuffer.h
        ccallsign.cpp ccallsign.h
        ccallsignlist.cpp ccallsignlist.h
        ccallsignlistitem.cpp ccallsignlistitem.h
        cclient.cpp cclient.h
        cclients.cpp cclients.h
        ccodecstream.cpp ccodecstream.h
        ccrc.cpp ccrc.h
        cdcsclient.cpp cdcsclient.h
        cdcsprotocol.cpp cdcsprotocol.h
        cdextraclient.cpp cdextraclient.h
        cdextraprotocol.cpp cdextraprotocol.h
        cdmriddir.cpp cdmriddir.h
        cdmriddirfile.cpp cdmriddirfile.h
        cdmriddirhttp.cpp cdmriddirhttp.h
        cdmrmmdvmclient.cpp cdmrmmdvmclient.h
        cdmrmmdvmprotocol.cpp cdmrmmdvmprotocol.h
        cdmrplusclient.cpp cdmrplusclient.h
        cdmrplusprotocol.cpp cdmrplusprotocol.h
        cdplusclient.cpp cdplusclient.h
        cdplusprotocol.cpp cdplusprotocol.h
        cdvframepacket.cpp cdvframepacket.h
        cdvheaderpacket.cpp cdvheaderpacket.h
        cdvlastframepacket.cpp cdvlastframepacket.h
        cgatekeeper.cpp cgatekeeper.h
        cgolay2087.cpp cgolay2087.h
        cgolay24128.cpp cgolay24128.h
        chamming.cpp chamming.h
        cip.cpp cip.h
        cnotification.cpp cnotification.h
        cnotificationqueue.h
        cpacket.cpp cpacket.h
        cpacketqueue.h
        cpacketstream.cpp cpacketstream.h
        cpeercallsignlist.cpp cpeercallsignlist.h
        cpeer.cpp cpeer.h
        cpeers.cpp cpeers.h
        cprotocol.cpp cprotocol.h
        cprotocols.cpp cprotocols.h
        cqr1676.cpp cqr1676.h
        creflector.cpp creflector.h
        crs129.cpp crs129.h
        csemaphore.cpp csemaphore.h
        ctimepoint.cpp ctimepoint.h
        ctranscoder.cpp ctranscoder.h
        cudpsocket.cpp cudpsocket.h
        cuser.cpp cuser.h
        cusers.cpp cusers.h
        cutils.cpp cutils.h
        cversion.cpp cversion.h
        cwiresxcmd.cpp cwiresxcmd.h
        cwiresxcmdhandler.cpp cwiresxcmdhandler.h
        cwiresxcmdqueue.h
        cwiresxinfo.cpp cwiresxinfo.h
        cwiresxpacket.h
        cwiresxpacketqueue.h
        cxlxclient.cpp cxlxclient.h
        cxlxpeer.cpp cxlxpeer.h
        cxlxprotocol.cpp cxlxprotocol.h
        cysfclient.cpp cysfclient.h
        cysfconvolution.cpp cysfconvolution.h
        cysffich.cpp cysffich.h
        cysfnode.cpp cysfnode.h
        cysfnodedir.cpp cysfnodedir.h
        cysfnodedirfile.cpp cysfnodedirfile.h
        cysfnodedirhttp.cpp cysfnodedirhttp.h
        cysfpayload.cpp cysfpayload.h
        cysfprotocol.cpp cysfprotocol.h
        cysfutils.cpp cysfutils.h
        main.cpp main.h
        syslog.h
        ysfdefines.h)

target_link_libraries(xlxd
        LINK_PUBLIC
        Threads::Threads)

target_compile_options(xlxd
        PRIVATE
        -Wall)
