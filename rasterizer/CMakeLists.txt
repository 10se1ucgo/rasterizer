cmake_minimum_required (VERSION 3.9)

add_executable(rasterizer "framebuffer.cpp" "framebuffer.h" "rasterizer.cpp" "window.cpp" "window.h")

set_property(TARGET rasterizer PROPERTY CXX_STANDARD 17)
set_property(TARGET rasterizer PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

include_directories(${SDL2_INCLUDE_DIR})
target_link_libraries(rasterizer fmt::fmt glm::glm ${SDL2_LIBRARY})

target_compile_definitions(rasterizer PUBLIC -DGLM_FORCE_AVX -DGLM_FORCE_ALIGNED_GENTYPES)

if(MSVC AND NOT (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
    target_compile_options(rasterizer PUBLIC /arch:AVX)
    set_target_properties(rasterizer PROPERTIES LINK_FLAGS "/profile")
else()
    target_compile_options(rasterizer PUBLIC -mavx)
endif()