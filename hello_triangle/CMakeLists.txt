add_executable(hello_triangle
    main.cpp
    app.h
    app.cpp)

target_link_libraries(hello_triangle PRIVATE glfw)
target_link_libraries(hello_triangle PRIVATE Vulkan::Vulkan)

target_include_directories(hello_triangle PRIVATE ${GLM_INCLUDE_DIR})

set(SHADER_SRC_FILES
    shader.vert
    shader.frag)

BuildShaderFiles(${SHADER_SRC_FILES})

add_custom_target(shaders ALL DEPENDS ${SHADER_SPIRV_FILES})