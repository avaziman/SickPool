cmake_minimum_required(VERSION 3.16.0)

# set(CMAKE_C_COMPILER /usr/bin/clang-14)
# set(CMAKE_CXX_COMPILER /usr/bin/clang-14)
set(CMAKE_C_COMPILER /usr/bin/gcc)
set(CMAKE_CXX_COMPILER /usr/bin/g++)

project("SickPool" CXX C)
set(PROJECT_NAME_CORE ${PROJECT_NAME}_core)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_BUILD_TYPE "Debug")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # -g3 maximum debugging information
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g3")
    add_compile_definitions(SIMDJSON_DEVELOPMENT_CHECKS)
else()
    # Makes the executable run the fastest on host cpu and enable optimizations
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -O3 -Ofast")
endif()

include(FetchContent)

FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG tags/v2.1.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(simdjson)

include(FetchContent)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG tags/9.0.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(fmt)

# GoogleTest requires at least C++14
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG tags/v1.12.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googletest)

# include_directories(lib/rapidjson/include)
add_subdirectory(src)

add_subdirectory(stratum_notifier)
# add_subdirectory(benchmark)
add_subdirectory(tests)
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME_CORE})