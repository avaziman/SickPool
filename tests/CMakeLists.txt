enable_testing()

get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)

file(GLOB_RECURSE SRC_FILES
     "${PARENT_DIR}/src/stratum/*.h"
     "${PARENT_DIR}/src/stratum/*.hpp"
     "${PARENT_DIR}/src/stratum/*.cpp"
     "${PARENT_DIR}/src/daemon/*.hpp"
     "${PARENT_DIR}/src/daemon/*.cpp"
     "${PARENT_DIR}/src/crypto/base58.cpp"
     "${PARENT_DIR}/src/crypto/block_header.hpp"
     "${PARENT_DIR}/src/crypto/verus_header.hpp"
     "${PARENT_DIR}/src/crypto/hash_wrapper.hpp"
     "${PARENT_DIR}/src/crypto/hash_wrapper.cpp"
     "${PARENT_DIR}/src/crypto/verus_transaction.hpp"
     "${PARENT_DIR}/src/crypto/verushash/*.h"
     "${PARENT_DIR}/src/crypto/verushash/*.cpp"
     "${PARENT_DIR}/src/crypto/verushash/*.c"
)

set_source_files_properties(${PARENT_DIR}/src/crypto/verushash/verus_clhash.cpp PROPERTIES COMPILE_FLAGS " -m64 -mavx -mpclmul -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -maes  -funroll-loops -fomit-frame-pointer")
set_source_files_properties(${PARENT_DIR}/src/crypto/verushash/verus_hash.cpp PROPERTIES COMPILE_FLAGS "   -m64 -mavx -mpclmul -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -maes  -funroll-loops -fomit-frame-pointer")
set_source_files_properties(${PARENT_DIR}/src/crypto/verushash/haraka.c PROPERTIES COMPILE_FLAGS "         -m64 -mavx -mpclmul -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -maes  -funroll-loops -fomit-frame-pointer")

include_directories(../src)

add_executable(${PROJECT_NAME}_tests 
    hash_wrapper_test.cpp
    block_hashing_test.cpp
    merkle_root_test.cpp
    coinbasetx_build_test.cpp
    daemon_rpc_test.cpp
    stratum_test.cpp
    ${SRC_FILES})

target_link_libraries(
    ${PROJECT_NAME}_tests
    gtest_main
)

if(WIN32)
target_link_libraries(
    ${PROJECT_NAME}_tests
    ws2_32
)
endif(WIN32)