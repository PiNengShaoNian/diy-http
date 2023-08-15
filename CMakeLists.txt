cmake_minimum_required(VERSION 3.0.0)
project(httpd VERSION 0.1.0 LANGUAGES C)

include(CTest)
enable_testing()

add_compile_options(-g)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
add_executable(httpd main.c httpd.c httpd.h)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
