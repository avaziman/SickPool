include_directories(daemon)
include_directories(stratum)
include_directories(.)

# optimized hash function
add_subdirectory(crypto/verushash)

file(GLOB SRC_FILES
     "main.cpp"
     "daemon/*.cpp"
     "stratum/*.cpp"
     "crypto/*.cpp"
     "stats_manager.cpp"
     )

add_executable(${PROJECT_NAME} ${SRC_FILES})

# <------------ add hiredis dependency here --------------->
find_path(HIREDIS_HEADER hiredis)
target_include_directories(${PROJECT_NAME} PUBLIC ${HIREDIS_HEADER})
find_library(HIREDIS_LIB hiredis)

target_link_libraries(
    ${PROJECT_NAME}
    ${HIREDIS_LIB}
    pthread
    simdjson
    fmt::fmt
    sickverushash
    
)