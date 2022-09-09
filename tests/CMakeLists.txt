cmake_minimum_required(VERSION 3.18)
enable_testing()
set(PROJECT_NAME_TESTS ${PROJECT_NAME}_tests)

# GoogleTest requires at least C++14
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG tags/v1.12.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googletest)

# include_directories(../src)
# include_directories(../src/daemon)
# include_directories(../src/stratum)

# SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g3")
# SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g3")
file(GLOB SRC_FILES
    main.cpp
    redis_test.cpp
    job_manager_test.cpp
    req_parse_test.cpp
    merkle_root_test.cpp
    difficulty_test.cpp
    difficulty_manager_test.cpp
    hash_wrapper_manager_test.cpp
    payment_test.cpp
    job_construction_test.cpp
    merkle_root_test.cpp
    stratum_test.cpp
)

add_executable(${PROJECT_NAME_TESTS} ${SRC_FILES})
target_link_libraries(${PROJECT_NAME_TESTS} ${PROJECT_NAME_CORE} gtest)

# gtest_discover_tests(${PROJECT_NAME_TESTS})
