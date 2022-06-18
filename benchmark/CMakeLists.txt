include_directories(../src/stratum)

get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)

set(CMAKE_CXX_STANDARD 17)

# file(GLOB_RECURSE SRC_FILES
#      "${PARENT_DIR}/src/*.h"
#      "${PARENT_DIR}/src/*.c"
#      "${PARENT_DIR}/src/*.hpp"
#      "${PARENT_DIR}/src/*.cpp"
# )

# list(REMOVE_ITEM SRC_FILES "${PARENT_DIR}/src/main.cpp")

# set_source_files_properties(${PARENT_DIR}/src/crypto/verushash/verus_clhash.cpp PROPERTIES COMPILE_FLAGS " -m64 -mavx -mpclmul -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -maes  -funroll-loops -fomit-frame-pointer")
# set_source_files_properties(${PARENT_DIR}/src/crypto/verushash/verus_hash.cpp PROPERTIES COMPILE_FLAGS "   -m64 -mavx -mpclmul -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -maes  -funroll-loops -fomit-frame-pointer")
# set_source_files_properties(${PARENT_DIR}/src/crypto/verushash/haraka.c PROPERTIES COMPILE_FLAGS "         -m64 -mavx -mpclmul -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -maes  -funroll-loops -fomit-frame-pointer")

include_directories(../src)

add_executable(${PROJECT_NAME}_bench 
                    # ${SRC_FILES}
                    # redis_bench.cpp
                    diff_bench.cpp
                    other_bench.cpp
                    #verus_hash_bench.cpp
                    )

target_link_libraries(${PROJECT_NAME}_bench
                        benchmark::benchmark
                        benchmark::benchmark_main
                        hiredis)
