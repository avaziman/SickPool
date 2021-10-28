cmake_minimum_required(VERSION 3.21.0)

project("SickPool" CXX)

set(CMAKE_CXX_STANDARD 11)

include_directories(lib/rapidjson/include)

add_subdirectory(src)
add_subdirectory(lib/googletest)
add_subdirectory(tests)
# add_subdirectory(tests)