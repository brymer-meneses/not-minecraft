
function(target_glsl_shaders TARGET_NAME)
  cmake_parse_arguments(
        GLSL
        ""
        ""
        "SOURCES;COMPILE_OPTIONS"
        ${ARGN})

  if(NOT GLSL_SOURCES)
    message(FATAL_ERROR "target_glsl_shaders: No SOURCES specified")
  endif()

  set(GENERATED_SPIRV_FILES)

  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders/")
  foreach(GLSL_FILE IN LISTS GLSL_SOURCES)
    if(IS_ABSOLUTE ${GLSL_FILE})
      set(GLSL_INPUT_PATH ${GLSL_FILE})
    else()
      set(GLSL_INPUT_PATH "${CMAKE_SOURCE_DIR}/${GLSL_FILE}")
    endif()

    set(SPIRV_OUTPUT "${CMAKE_BINARY_DIR}/${GLSL_FILE}.spv")

    get_filename_component(GLSL_FILE_NAME ${GLSL_FILE} NAME)
    add_custom_command(
      OUTPUT ${SPIRV_OUTPUT}
      COMMAND ${GLSL_VALIDATOR_EXE} ${GLSL_COMPILE_OPTIONS} -V
              "${CMAKE_SOURCE_DIR}/${GLSL_FILE}" -o ${SPIRV_OUTPUT}
      MAIN_DEPENDENCY ${GLSL_FILE}
      COMMENT "Compiling GLSL shader: ${GLSL_FILE_NAME}"
    )
    list(APPEND GENERATED_SPIRV_FILES "${SPIRV_OUTPUT}")
  endforeach()

  target_sources(${TARGET_NAME} PRIVATE ${GENERATED_SPIRV_FILES})

  # We are using `#embed` so make sure that the C++ compiler can see it.
  target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_BINARY_DIR}/src/")
endfunction()
