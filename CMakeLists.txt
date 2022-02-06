cmake_minimum_required(VERSION 3.16.0)

project("SickPool" CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)

include_directories(lib/rapidjson/include)
include_directories(lib/redis-plus-plus/include)

add_subdirectory(src)
add_subdirectory(block_notifier)
add_subdirectory(lib/googletest)
add_subdirectory(lib/redis-plus-plus)
# add_subdirectory(tests)