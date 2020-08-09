cmake_minimum_required (VERSION 3.9)

add_executable (rasterizer "framebuffer.cpp" "framebuffer.h")
set_property(TARGET rasterizer PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

target_link_libraries(rasterizer fmt::fmt glm::glm)
if(MSVC AND NOT (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
    target_compile_options(rasterizer PUBLIC /arch:AVX)
    set_target_properties(rasterizer PROPERTIES LINK_FLAGS "/profile")
else()
    target_compile_options(rasterizer PUBLIC -msse4.2)
endif()