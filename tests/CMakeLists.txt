enable_testing()
include(GoogleTest)

# include_directories(../src)
include_directories(../src/daemon)
include_directories(../src/stratum)

set(PROJECT_NAME_TESTS ${PROJECT_NAME}_tests)

add_executable(${PROJECT_NAME_TESTS}
        main.cpp
        redis_test.cpp
        job_manager_test.cpp
        req_parse_test.cpp
        # difficulty_test.cpp
        # job_construction_test.cpp
        # hash_wrapper_test.cpp
        # merkle_root_test.cpp
        # difficulty_test.cpp
        #stratum_test.cpp
        )


target_link_libraries(
    ${PROJECT_NAME_TESTS}
    ${PROJECT_NAME_CORE} # link the pool
    gtest
)
gtest_discover_tests(${PROJECT_NAME_TESTS})
