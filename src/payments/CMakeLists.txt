cmake_minimum_required(VERSION 3.0)
project("payment_manager" C CXX)

add_executable(${PROJECT_NAME} main.cpp payment_manager.cpp)
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME_CORE})