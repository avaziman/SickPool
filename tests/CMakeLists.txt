enable_testing()
include(GoogleTest)

# include_directories(../src)
include_directories(../src/daemon)
include_directories(../src/stratum)

set(PROJECT_NAME_TESTS ${PROJECT_NAME}_tests)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g3")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g3")


add_executable(${PROJECT_NAME_TESTS}
    main.cpp
    ./jobs/job_vrsc_test.cpp
    redis_test.cpp
    job_manager_test.cpp
    req_parse_test.cpp
    merkle_root_test.cpp
    difficulty_test.cpp
    hash_wrapper_test.cpp
    payment_test.cpp
    # job_construction_test.cpp
    # merkle_root_test.cpp
    # stratum_test.cpp
)

target_link_libraries(
    ${PROJECT_NAME_TESTS}
    ${PROJECT_NAME_CORE} # link the pool
    gtest
)
# gtest_discover_tests(${PROJECT_NAME_TESTS})
