cmake_minimum_required(VERSION 4.0)
project(not-minecraft)

include(cmake/dependencies.cmake)
include(cmake/utils.cmake)

add_executable(not-minecraft src/main.cpp)
target_sources(not-minecraft 
  PUBLIC FILE_SET CXX_MODULES FILES
  src/engine.cpp
)
target_compile_features(not-minecraft PUBLIC cxx_std_23)
target_link_libraries(not-minecraft PRIVATE Vulkan::Vulkan glfw)
target_glsl_shaders(not-minecraft 
  SOURCES 
  src/shaders/shader.vert
)

