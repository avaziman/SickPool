cmake_minimum_required(VERSION 3.18)

include(FetchContent)

FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG tags/v2.2.2
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(simdjson)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG tags/9.0.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(fmt)

# optimized hash function
add_subdirectory(crypto/verushash)
add_subdirectory(crypto/x25x)
find_path(HIREDIS_HEADER hiredis)

file(GLOB SRC_FILES
    "stratum/*.cpp"
    "crypto/*.cpp"
    "stats/*.cpp"
    "blocks/*.cpp"
    "jobs/*.cpp"
    "redis/*.cpp"
    "round/*.cpp"
    "payments/*.cpp"
    "logger/*.cpp"
)

# message("SRC FILES: ${SRC_FILES}")

add_library(${PROJECT_NAME_CORE} ${SRC_FILES})

set(SICKPOOL_INCLUDE PUBLIC . PUBLIC stats PUBLIC crypto PUBLIC daemon PUBLIC stratum PUBLIC static_config PUBLIC blocks PUBLIC round PUBLIC redis PUBLIC payments PUBLIC logger PUBLIC shares PUBLIC jobs)
target_include_directories(${PROJECT_NAME_CORE} PUBLIC ${SICKPOOL_INCLUDE})

find_library(HIREDIS_LIB hiredis)
target_include_directories(${PROJECT_NAME_CORE} PUBLIC ${HIREDIS_HEADER})

target_link_libraries(
    ${PROJECT_NAME_CORE}
    ${HIREDIS_LIB}
    pthread
    simdjson
    fmt::fmt
    sickverushash
    sickx25x
)