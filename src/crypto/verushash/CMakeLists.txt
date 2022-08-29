cmake_minimum_required(VERSION 3.16.0)
project("sickverushash" CXX C)

# verushash should always be optimized, (we don't debug it)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -O3")
set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -march=native -O3")

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/verus_clhash.cpp PROPERTIES COMPILE_FLAGS " -mpclmul -msse4 -msse4.1 -msse4.2 -mssse3 -mavx -maes -g -funroll-loops -fomit-frame-pointer")
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/verus_hash.cpp PROPERTIES COMPILE_FLAGS " -mpclmul -msse4 -msse4.1 -msse4.2 -mssse3 -mavx -maes -g -funroll-loops -fomit-frame-pointer")
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/haraka.c PROPERTIES COMPILE_FLAGS " -mpclmul -msse4 -msse4.1 -msse4.2 -mssse3 -mavx -maes -g -funroll-loops -fomit-frame-pointer")

file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}
    "*.c"
    "*.cpp"
    )

add_library(${PROJECT_NAME} STATIC ${SRC_FILES})