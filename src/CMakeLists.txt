cmake_minimum_required(VERSION 3.18)

execute_process(COMMAND cxxbridge /home/sickguy/Documents/Projects/SickPool/api_rust/src/redis_interop.rs
                OUTPUT_FILE ./redis/redis_interop.hpp
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

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
    "persistence/redis/*.cpp"
    "persistence/mysql/*.cpp"
    "round/*.cpp"
    "payments/*.cpp"
    "logger/*.cpp"
    "daemon/*.cpp"
)

# dependencies
# BOOST
FIND_PACKAGE(Boost 1.40 COMPONENTS program_options REQUIRED)

include(FindPkgConfig)

add_library(${PROJECT_NAME_CORE} ${SRC_FILES})

set(SICKPOOL_INCLUDE PUBLIC . PUBLIC stats PUBLIC crypto PUBLIC daemon PUBLIC stratum PUBLIC static_config PUBLIC blocks PUBLIC round PUBLIC persistence/redis PUBLIC payments PUBLIC logger PUBLIC shares PUBLIC jobs PUBLIC difficulty PUBLIC control)
target_include_directories(${PROJECT_NAME_CORE} PUBLIC ${SICKPOOL_INCLUDE})

target_include_directories(${PROJECT_NAME_CORE} PUBLIC ${HIREDIS_HEADER})

target_link_libraries(
    ${PROJECT_NAME_CORE}
    ${Boost_LIBRARIES}
    hiredis
    # /usr/lib/x86_64-linux-gnu/libmysqlcppconn-static.a
    # /usr/lib/x86_64-linux-gnu/libmysqlcppconn.a
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
