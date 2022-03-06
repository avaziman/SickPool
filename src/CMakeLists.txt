include_directories(daemon)
include_directories(stratum)

file(GLOB_RECURSE SRC_FILES 
     "*.h"
     "*.c"
     "*.hpp"
     "*.cpp"
     "../lib/simdjson.cpp"
     )


find_package(Boost 1.68 REQUIRED)

add_executable(${PROJECT_NAME} ${SRC_FILES})

# <------------ add hiredis dependency here --------------->
find_path(HIREDIS_HEADER hiredis)
target_include_directories(${PROJECT_NAME} PUBLIC ${HIREDIS_HEADER})

find_library(HIREDIS_LIB hiredis)

# <------------ add redis-plus-plus dependency here -------------->
find_path(REDIS_PLUS_PLUS_HEADER sw)      # NOTE: this should be *sw* NOT *redis++*
target_include_directories(${PROJECT_NAME} PUBLIC ${REDIS_PLUS_PLUS_HEADER})

find_library(REDIS_PLUS_PLUS_LIB redis++)

target_link_libraries(
    ${PROJECT_NAME}
    ${HIREDIS_LIB}
    ${REDIS_PLUS_PLUS_LIB}
    Boost::boost
    pthread
)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/crypto/verushash/verus_clhash.cpp PROPERTIES COMPILE_FLAGS " -m64 -mavx -mpclmul -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -maes  -funroll-loops -fomit-frame-pointer")
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/crypto/verushash/verus_hash.cpp PROPERTIES COMPILE_FLAGS "   -m64 -mavx -mpclmul -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -maes  -funroll-loops -fomit-frame-pointer")
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/crypto/verushash/haraka.c PROPERTIES COMPILE_FLAGS "         -m64 -mavx -mpclmul -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -maes  -funroll-loops -fomit-frame-pointer")