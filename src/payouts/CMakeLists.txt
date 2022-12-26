cmake_minimum_required(VERSION 3.0)
project("payout_manager" C CXX)

add_executable(${PROJECT_NAME} main.cpp payout_manager.cpp)
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME_CORE})