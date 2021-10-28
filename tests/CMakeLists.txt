enable_testing()

get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)

file(GLOB_RECURSE SRC_FILES
     "${PARENT_DIR}/src/stratum/*.h"
     "${PARENT_DIR}/src/stratum/*.hpp"
     "${PARENT_DIR}/src/stratum/*.cpp"
)

include_directories(../src)
add_executable(${PROJECT_NAME}_tests stratum_tests.cpp ${SRC_FILES})

target_link_libraries(
    ${PROJECT_NAME}_tests
    gtest_main
    ws2_32
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_tests)