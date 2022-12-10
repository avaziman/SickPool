cmake_minimum_required(VERSION 3.16.0)

project("stratum_notifier" CXX)

add_subdirectory(stratum_notifier_lib)
add_executable(${PROJECT_NAME} stratum_notifier.cpp)
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}_lib)
