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

find_program(GLSL_VALIDATOR_EXE "glslangValidator")
mark_as_advanced(FORCE GLSL_VALIDATOR_EXE)
if(GLSL_VALIDATOR_EXE)
  message(STATUS "glslangValidator found: ${GLSLANGVALIDATOR_EXE}")
else()
  message(FATAL_ERROR "glslangValidator not found!")
endif()

