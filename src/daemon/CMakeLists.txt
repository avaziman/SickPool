cmake_minimum_required(VERSION 3.16.0)
project("daemon_rpc" C CXX)

include(FetchContent)
FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG tags/v2.2.2
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(simdjson)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG tags/9.1.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(fmt)

file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}
    "*.c"
    "*.cpp"
)

add_library(${PROJECT_NAME} STATIC ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} simdjson fmt::fmt)
target_include_directories(${PROJECT_NAME} PUBLIC .)
