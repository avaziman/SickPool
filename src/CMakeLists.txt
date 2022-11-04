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
    GIT_TAG tags/9.1.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(fmt)

# optimized hash function
add_subdirectory(crypto/cn)
add_subdirectory(crypto/verushash)
# add_subdirectory(daemon)
# add_subdirectory(crypto/x25x)
find_path(HIREDIS_HEADER hiredis)

file(GLOB SRC_FILES
    "shares/*.cpp"
    "stratum/*.cpp"
    "crypto/*.cpp"

    # "crypto/cn/crypto/*.c"
    # "crypto/cn/ethash/*.c"
    "stats/*.cpp"
    "blocks/*.cpp"
    "jobs/*.cpp"
    "redis/*.cpp"
    "round/*.cpp"
    "payments/*.cpp"
    "logger/*.cpp"
    "daemon/*.cpp"
)

message("SRC FILES: ${SRC_FILES}")
FIND_PACKAGE(Boost 1.40 COMPONENTS program_options REQUIRED )

add_library(${PROJECT_NAME_CORE} ${SRC_FILES})

set(SICKPOOL_INCLUDE PUBLIC . PUBLIC stats PUBLIC crypto PUBLIC daemon PUBLIC stratum PUBLIC static_config PUBLIC blocks PUBLIC round PUBLIC redis PUBLIC payments PUBLIC logger PUBLIC shares PUBLIC jobs PUBLIC difficulty PUBLIC control)
target_include_directories(${PROJECT_NAME_CORE} PUBLIC ${SICKPOOL_INCLUDE})

find_library(HIREDIS_LIB hiredis)
target_include_directories(${PROJECT_NAME_CORE} PUBLIC ${HIREDIS_HEADER})

target_link_libraries(
    ${PROJECT_NAME_CORE}
    ${HIREDIS_LIB}
    ${Boost_LIBRARIES}
    pthread
    cryptonote_utils
    simdjson
    fmt::fmt
    sickverushash
    # sickx25x
)

# add_subdirectory(blocks)
# add_subdirectory(daemon)
# add_subdirectory(payments)
