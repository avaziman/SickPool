cmake_minimum_required(VERSION 3.23)
project("block_watcher" C CXX)

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

add_executable(${PROJECT_NAME} main.cpp)

# target_include_directories(${PROJECT_NAME} PUBLIC ../static_config ../daemon ../logger ../redis ../jobs ../crypto ../round ../payments ../stats ../stratum .. .)
# target_include_directories(${PROJECT_NAME} PUBLIC ${SICKPOOL_INCLUDE})
# TODO: remove all useless dependencies, (CORE)
target_link_libraries(${PROJECT_NAME} simdjson fmt::fmt ${PROJECT_NAME_CORE})