cmake_minimum_required(VERSION 3.9)

project("rasterizer")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")
set(CMAKE_PREFIX_PATH "ext/sdl2" CACHE PATH "" FORCE)

find_package(SDL2 REQUIRED)

add_subdirectory("rasterizer")
add_subdirectory("ext/fmt")
add_subdirectory("ext/glm")