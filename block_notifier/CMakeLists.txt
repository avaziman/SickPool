# SET(BLOCK_NOTIFIER ${CMAKE_CURRENT_BINARY_DIR}/block_notifier)

add_executable(block_notifier block_notifier.cpp)

if(WIN32) 
target_link_libraries(
    block_notifier
    ws2_32
    )
endif(WIN32)