include_directories(daemon)

file(GLOB_RECURSE SRC_FILES
     "*.h"
     "*.c"
     "*.hpp"
     "*.cpp"
     )

find_package(Boost 1.68 REQUIRED)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_link_libraries(
    ${PROJECT_NAME}
    Boost::boost
    pthread
    )

if(WIN32) 
target_link_libraries(
    ${PROJECT_NAME}
    ws2_32
)
endif(WIN32)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/crypto/verushash/verus_clhash.cpp PROPERTIES COMPILE_FLAGS " -m64 -mavx -mpclmul -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -maes  -funroll-loops -fomit-frame-pointer")
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/crypto/verushash/verus_hash.cpp PROPERTIES COMPILE_FLAGS "   -m64 -mavx -mpclmul -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -maes  -funroll-loops -fomit-frame-pointer")
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/crypto/verushash/haraka.c PROPERTIES COMPILE_FLAGS "         -m64 -mavx -mpclmul -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -maes  -funroll-loops -fomit-frame-pointer")
