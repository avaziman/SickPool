cmake_minimum_required(VERSION 3.16.0)

project("SickPool" CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_BUILD_TYPE "Debug")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    #-g3 maximum debugging information
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g3")
    add_compile_definitions(SIMDJSON_DEVELOPMENT_CHECKS)
else()
    # Makes the executable run the fastest on host cpu and enable optimizations
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -O3 -Ofast")
endif()

# include_directories(lib/rapidjson/include)
add_subdirectory(lib/simdjson)

add_subdirectory(src)
add_subdirectory(stratum_notifier)
# add_subdirectory(benchmark)

# add_subdirectory(lib/googletest)
# add_subdirectory(lib/benchmark)

# add_subdirectory(tests)