cmake_minimum_required(VERSION 3.16.0)
project("cryptonote_utils" C CXX)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
SET(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -O3")

file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}
    "*.c"
    "*.cpp"
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Position Independent Code
    add_library(${PROJECT_NAME} STATIC ${SRC_FILES})
else()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    add_library(${PROJECT_NAME} SHARED ${SRC_FILES})
endif()

# message("Cryptonote build type: ${CMAKE_BUILD_TYPE}")
# message("cryptonote src files: ${SRC_FILES}")


target_include_directories(${PROJECT_NAME} PUBLIC "common" PUBLIC "crypto" PUBLIC . PUBLIC epee/include PUBLIC currency_core)

