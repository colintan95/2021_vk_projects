add_executable(point_light
    main.cpp
    app.h
    app.cpp)

target_link_libraries(point_light PRIVATE glfw)
target_link_libraries(point_light PRIVATE Vulkan::Vulkan)

target_include_directories(point_light PRIVATE ${GLM_INCLUDE_DIR})

set(SHADER_SRC_FILES
    shader.vert
    shader.frag)

build_shader_files(TARGET point_light FILES ${SHADER_SRC_FILES})

add_custom_target(shaders ALL DEPENDS ${SHADER_SPIRV_FILES})