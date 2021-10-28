include_directories(daemon)

file(GLOB_RECURSE SRC_FILES
     "*.h"
     "*.hpp"
     "*.cpp"
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

if(WIN32) 
target_link_libraries(
    ${PROJECT_NAME}
    ws2_32
    )
endif(WIN32)