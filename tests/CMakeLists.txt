enable_testing()
include(GoogleTest)

set(CMAKE_CXX_STANDARD 20)

include_directories(../src)

add_executable(${PROJECT_NAME}_tests 
        main.cpp
        database.cpp
        # difficulty_test.cpp
        # job_construction_test.cpp
        # hash_wrapper_test.cpp
        # merkle_root_test.cpp
        # difficulty_test.cpp
        #stratum_test.cpp
        )

# <------------ add hiredis dependency here --------------->
find_path(HIREDIS_HEADER hiredis)
target_include_directories(${PROJECT_NAME}_tests PUBLIC ${HIREDIS_HEADER})

find_library(HIREDIS_LIB hiredis)

set_property(TARGET ${PROJECT_NAME} PROPERTY ENABLE_EXPORTS 1)

target_link_libraries(
    ${PROJECT_NAME}_tests
    ${PROJECT_NAME} # link the pool
    ${HIREDIS_LIB}
    gtest
)
gtest_discover_tests(${PROJECT_NAME}_tests)
