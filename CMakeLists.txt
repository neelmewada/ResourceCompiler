cmake_minimum_required(VERSION 3.22)
project(ResourceCompiler)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE RESOURCE_SRC resources/*.c)

add_executable(${PROJECT_NAME} src/main.cpp src/test.cpp)# ${RESOURCE_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE )#${CMAKE_SOURCE_DIR}/resources)

#target_link_libraries(${PROJECT_NAME} resources)
