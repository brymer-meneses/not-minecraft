cmake_minimum_required(VERSION 4.0)
project(not-minecraft)

include(FetchContent)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

find_package(Vulkan REQUIRED)

add_executable(not-minecraft src/main.cpp)
target_sources(not-minecraft 
  PUBLIC FILE_SET CXX_MODULES FILES
  src/engine.cpp
)
target_compile_features(not-minecraft PUBLIC cxx_std_23)
target_link_libraries(not-minecraft PRIVATE  Vulkan::Vulkan glfw)

