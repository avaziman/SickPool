# optimized hash function
add_subdirectory(crypto/verushash)
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

add_library(${PROJECT_NAME_CORE} ${SRC_FILES})

set(SICKPOOL_INCLUDE PUBLIC . PUBLIC stats PUBLIC crypto PUBLIC daemon PUBLIC stratum PUBLIC static_config PUBLIC blocks PUBLIC round PUBLIC redis PUBLIC payments PUBLIC logger)
target_include_directories(${PROJECT_NAME_CORE} ${SICKPOOL_INCLUDE})

find_library(HIREDIS_LIB hiredis)
target_include_directories(${PROJECT_NAME_CORE} PUBLIC ${HIREDIS_HEADER})

target_link_libraries(
    ${PROJECT_NAME_CORE}
    ${HIREDIS_LIB}
    pthread
    simdjson
    fmt::fmt
    sickverushash
)