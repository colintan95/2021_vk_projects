add_executable(hello_triangle
    main.cpp
    app.h
    app.cpp)

target_link_libraries(hello_triangle PRIVATE glfw)
target_link_libraries(hello_triangle PRIVATE Vulkan::Vulkan)

target_include_directories(hello_triangle PRIVATE ${GLM_INCLUDE_DIR})

set(SHADER_SRC
    shader.vert
    shader.frag)

function(BuildShaderFiles)   
  set(GLSL_COMPILER_PATH "/Users/colintan/VulkanSDK/1.2.189.0/macOs/bin/glslc")

  set(SHADER_SPIRV_FILES)

  foreach(file ${ARGV})
    string(REGEX REPLACE "(.*).(vert|frag)" "\\1_\\2" new_name ${file})

    string(CONCAT new_path ${new_name} ".spv")

    add_custom_command(OUTPUT ${new_path}
      COMMAND "${GLSL_COMPILER_PATH}" "${file}" -o "$<TARGET_FILE_DIR:hello_triangle>/${new_path}"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      MAIN_DEPENDENCY ${file}
      COMMENT "Creating ${new_path}")

    list(APPEND SHADER_SPIRV_FILES ${new_path})
  endforeach()

  set(SHADER_SPIRV_FILES ${SHADER_SPIRV_FILES} PARENT_SCOPE)
endfunction()

BuildShaderFiles(${SHADER_SRC})
add_custom_target(shaders ALL DEPENDS ${SHADER_SPIRV_FILES})