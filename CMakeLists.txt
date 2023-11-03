cmake_minimum_required(VERSION 3.1)
project(xlxd)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

find_package(Threads REQUIRED)
message("Threads flags: ${CMAKE_THREAD_LIBS_INIT}")

add_subdirectory(src)
add_subdirectory(ambed)
add_subdirectory(ambedtest)
