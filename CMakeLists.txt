cmake_minimum_required(VERSION 3.18)

# set(CMAKE_C_COMPILER /usr/bin/clang-15)
# set(CMAKE_CXX_COMPILER /usr/bin/clang-15)
# set(CMAKE_C_COMPILER /usr/bin/gcc)
# set(CMAKE_CXX_COMPILER /usr/bin/g++)

# set(CMAKE_C_COMPILER /usr/bin/gcc-11)
# set(CMAKE_CXX_COMPILER /usr/bin/g++-11)
set(CMAKE_C_COMPILER /usr/bin/gcc-12)
set(CMAKE_CXX_COMPILER /usr/bin/g++-12)

project("SickPool" CXX C)
set(PROJECT_NAME_CORE ${PROJECT_NAME}_core)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Get the current working branch
execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

# Get the latest commit hash
execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # set(CMAKE_EXE_LINKER_FLAGS "-static")
    # KEEP DEBUG SYMBOLS 
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g3 -static-libstdc++")
else()
    # -g3 maximum debugging information
    # link time optimization
    SET(BUILD_SHARED_LIBS ON)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g3 -DDEBUG=1")
    message("Compile CXX options: ${CMAKE_CXX_FLAGS}")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)

# if (DEFINED WITH_UNIT_TEST)
    add_subdirectory(tests)
# endif()

if (DEFINED WITH_NOTIFIER)
    add_subdirectory(stratum_notifier)
endif()

# add_subdirectory(src/blocks)
# add_subdirectory(src/payouts)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME_CORE})
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")

# FetchContent_Declare(
#     benchmark
#     GIT_REPOSITORY https://github.com/google/benchmark.git
#     GIT_TAG tags/v1.6.1
#     GIT_SHALLOW TRUE
# )
# FetchContent_MakeAvailable(benchmark)
# add_subdirectory(benchmark)