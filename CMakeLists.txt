cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

project(vk_projects)

set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(GLFW_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw")
add_subdirectory(${GLFW_SRC_DIR})

set(GLM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm")

find_package(Vulkan REQUIRED FATAL_ERROR)

function(BUILD_SHADER_FILES)
  cmake_parse_arguments(BUILD_SHADER_FILES "" "TARGET" "FILES" ${ARGN})

  set(result)
  foreach(file ${BUILD_SHADER_FILES_FILES})
    string(REGEX REPLACE "(.*).(vert|frag)" "\\1_\\2" new_name ${file})
    string(CONCAT new_path ${new_name} ".spv")

    add_custom_command(OUTPUT ${new_path}
        COMMAND "${Vulkan_GLSLC_EXECUTABLE}" "${file}"
            -o "$<TARGET_FILE_DIR:${BUILD_SHADER_FILES_TARGET}>/${new_path}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        MAIN_DEPENDENCY ${file}
        COMMENT "Creating ${new_path}")

    list(APPEND result ${new_path})
  endforeach()

  set(BUILD_SPIRV_FILES ${result} PARENT_SCOPE)
endfunction()

function(COPY_ASSET_FILES)
  cmake_parse_arguments(COPY_ASSET_FILES "" "TARGET" "FILES" ${ARGN})

  set(result)
  foreach(file ${COPY_ASSET_FILES_FILES})
    add_custom_command(OUTPUT ${file}
        COMMAND ${CMAKE_COMMAND} -E copy "${file}"
            "$<TARGET_FILE_DIR:${COPY_ASSET_FILES_TARGET}>/${file}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        MAIN_DEPENDENCY ${file}
        COMMENT "Creating ${file} in build directory.")
    list(APPEND result ${file})
  endforeach()

  set(BUILD_ASSET_FILES ${result} PARENT_SCOPE)
endfunction()

add_subdirectory(utils)

add_subdirectory(point_light)