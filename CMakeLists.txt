cmake_minimum_required(VERSION 3.16.0)

project("SickPool" CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_BUILD_TYPE "Debug")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    #-g extends debugging information
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
else()
    # Makes the executable run the fastest on host cpu and enable optimizations
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -O3 -Ofast -DNDEBUG")
endif()

include_directories(lib/rapidjson/include)

add_subdirectory(src)
# add_subdirectory(block_notifier)
add_subdirectory(lib/googletest)
add_subdirectory(lib/redis-plus-plus)
# add_subdirectory(tests)