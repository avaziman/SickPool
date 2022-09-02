add_custom_target(bench)
include_directories(../src/stratum)

get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)

set(PROJECT_NAME_BENCH ${PROJECT_NAME}_bench)

add_executable(${PROJECT_NAME_BENCH}

    # ${SRC_FILES}
    # redis_bench.cpp
    submit.cpp
    diff_bench.cpp
    other_bench.cpp
    # verus_hash_bench.cpp
)

target_link_libraries(${PROJECT_NAME_BENCH}
    ${PROJECT_NAME_CORE}
    benchmark::benchmark
    benchmark::benchmark_main
)
