cmake_minimum_required(VERSION 3.18)

execute_process(COMMAND cxxbridge /home/sickguy/Documents/Projects/SickPool/api_rust/src/redis_interop.rs
                OUTPUT_FILE ./persistence/redis/redis_interop.hpp
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

FetchContent_Declare(
    hiredis
    GIT_REPOSITORY https://github.com/redis/hiredis.git
    GIT_TAG tags/v1.1.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(hiredis)

FetchContent_Declare(
    mysqlcppconn
    GIT_REPOSITORY https://github.com/mysql/mysql-connector-cpp.git
    GIT_TAG tags/8.0.31
    GIT_SHALLOW TRUE)
FetchContent_GetProperties(mysqlcppconn)

# optimized hash function
add_subdirectory(crypto/cn)
add_subdirectory(crypto/verushash)

# add_subdirectory(daemon)
# add_subdirectory(crypto/x25x)

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
    "persistence/*.cpp"
    "round/*.cpp"
    "payments/*.cpp"
    "logger/*.cpp"
    "daemon/*.cpp"
)

# dependencies
# BOOST
FIND_PACKAGE(Boost 1.40 COMPONENTS program_options REQUIRED)

add_library(${PROJECT_NAME_CORE} STATIC ${SRC_FILES})

set(SICKPOOL_INCLUDE PUBLIC . PUBLIC stats PUBLIC crypto PUBLIC daemon PUBLIC stratum
 PUBLIC static_config PUBLIC blocks PUBLIC round
 PUBLIC persistence/redis PUBLIC persistence/mysql PUBLIC persistence
 PUBLIC payments PUBLIC logger PUBLIC shares PUBLIC jobs PUBLIC difficulty PUBLIC control)
target_include_directories(${PROJECT_NAME_CORE} PUBLIC ${SICKPOOL_INCLUDE})

# find_path(HIREDIS_HEADER hiredis)
# find_library(HIREDIS_LIB hiredis)
target_include_directories(${PROJECT_NAME_CORE} PUBLIC ${HIREDIS_HEADER} ${mysqlcppconn_SOURCE_DIR}/include)

target_link_libraries(
    ${PROJECT_NAME_CORE}
    ${Boost_LIBRARIES}

    # ${HIREDIS_LIB}
    hiredis_static
    mysqlcppconn
    # mysqlcppconn-static
    pthread
    cryptonote_utils
    simdjson
    fmt::fmt
    sickverushash
    # sickx25x
)