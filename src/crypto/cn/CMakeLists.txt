cmake_minimum_required(VERSION 3.16.0)
project("cryptonote_utils" C CXX)

file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}
    "*.c"
    "*.cpp"
)

# message("cryptonote src files: ${SRC_FILES}")

add_library(${PROJECT_NAME} STATIC ${SRC_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC "common" PUBLIC "crypto" PUBLIC . PUBLIC epee/include PUBLIC currency_core)
