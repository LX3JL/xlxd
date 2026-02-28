add_executable(ambedtest
        cambe.cpp cambe.h
        cbuffer.cpp cbuffer.h
        ccodecstream.cpp ccodecstream.h
        cip.cpp cip.h
        csemaphore.cpp csemaphore.h
        ctimepoint.cpp ctimepoint.h
        ctranscoder.cpp ctranscoder.h
        cudpsocket.cpp cudpsocket.h
        main.cpp main.h
        samples.h)

target_link_libraries(ambedtest
        LINK_PUBLIC
        Threads::Threads)

target_compile_options(ambed
        PRIVATE
        -Wall)

add_test(ambedtest
        ambedtest)
