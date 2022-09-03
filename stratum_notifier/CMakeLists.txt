cmake_minimum_required(VERSION 3.16.0)

project("stratum_notifier" CXX)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    #-g extends debugging information
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
else()
    # Makes the executable run the fastest on host cpu and enable optimizations
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -O3 -Ofast -DNDEBUG")
endif()

add_executable(${PROJECT_NAME} stratum_notifier.cpp)
