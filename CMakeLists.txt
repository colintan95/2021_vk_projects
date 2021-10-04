cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

project(vk_projects)

set(GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw")
add_subdirectory(${GLFW_DIR})

set(GLM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm")

find_package(Vulkan REQUIRED FATAL_ERROR)

function(BUILD_SHADER_FILES)
  cmake_parse_arguments(BUILD_SHADER_FILES "" "TARGET" "FILES" ${ARGN})

  set(GLSL_COMPILER_PATH "/Users/colintan/VulkanSDK/1.2.189.0/macOs/bin/glslc")
  set(SHADER_SPIRV_FILES)

  foreach(file ${BUILD_SHADER_FILES_FILES})
    string(REGEX REPLACE "(.*).(vert|frag)" "\\1_\\2" new_name ${file})
    string(CONCAT new_path ${new_name} ".spv")

    add_custom_command(OUTPUT ${new_path}
      COMMAND "${GLSL_COMPILER_PATH}" "${file}"
          -o "$<TARGET_FILE_DIR:${BUILD_SHADER_FILES_TARGET}>/${new_path}"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      MAIN_DEPENDENCY ${file}
      COMMENT "Creating ${new_path}")

    list(APPEND SHADER_SPIRV_FILES ${new_path})
  endforeach()

  set(SHADER_SPIRV_FILES ${SHADER_SPIRV_FILES} PARENT_SCOPE)
endfunction()

add_subdirectory(hello_triangle)
