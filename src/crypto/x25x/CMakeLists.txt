cmake_minimum_required(VERSION 3.16.0)
project("sickx25x" CXX C)

# x25x should always be optimized, (we don't debug it)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")

# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -O3")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3 -O0")


# set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/verus_hash.cpp PROPERTIES COMPILE_FLAGS " -mpclmul -msse4 -msse4.1 -msse4.2 -mssse3 -mavx -maes -g -funroll-loops -fomit-frame-pointer")
# set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/haraka.c PROPERTIES COMPILE_FLAGS " -mpclmul -msse4 -msse4.1 -msse4.2 -mssse3 -mavx -maes -g -funroll-loops -fomit-frame-pointer")
file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}
    "x25x_module.c"
    "x25x.c"
    "sph/extra.c"
    "sph/lane.c"
    "sph/panama.c"
    "sph/blake.c"
    "sph/blake2s.c"
    "sph/echo.c"
    "sph/groestl.c"
    "sph/haval.c"
    "sph/keccak.c"
    "sph/shavite.c"
    "sph/SWIFFTX.c"
    "sph/bmw.c"
    "sph/fugue.c"
    "sph/hamsi.c"
    "sph/luffa.c"
    "sph/simd.c"
    "sph/sha2.c"
    "sph/sph_sha2.c"
    "sph/sph_sha2big.c"
    "sph/whirlpool.c"
    "sph/cubehash.c"
    "sph/gost_streebog.c"
    "sph/jh.c"
    "sph/sponge.c"
    "sph/lyra2.c"
    "sph/shabal.c"
    "sph/skein.c"
    "sph/tiger.c"
)

# set_source_files_properties(${SRC_FILES} PROPERTIES COMPILE_FLAGS " -fno-strict-aliasing -flto=auto -ffat-lto-objects -fstack-protector-strong -fPIC")
set_source_files_properties(${SRC_FILES} PROPERTIES COMPILE_FLAGS " -fno-strict-aliasing -Wdate-time -D_FORTIFY_SOURCE=2 -g -flto=auto -ffat-lto-objects -flto=auto -ffat-lto-objects -fstack-protector-strong -Wformat -Werror=format-security -fPIC")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing -flto=auto -ffat-lto-objects -fstack-protector-strong -fPIC ")

message(" X25 C_FLAGS: ${CMAKE_C_FLAGS} ")
# message(" X25X src files: ${SRC_FILES} ")
add_library(${PROJECT_NAME} STATIC ${SRC_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC " ./sph ")
target_include_directories(${PROJECT_NAME} PUBLIC " . ")