add_executable(point_light
    main.cpp
    app.h
    app.cpp)

target_include_directories(point_light PRIVATE "${PROJECT_ROOT_DIR}")

target_link_libraries(point_light PRIVATE glfw)
target_link_libraries(point_light PRIVATE Vulkan::Vulkan)

target_include_directories(point_light PRIVATE "${GLM_INCLUDE_DIR}")

target_link_libraries(point_light PRIVATE utils)

set(SHADER_SRC_FILES
    shader.frag
    shader.vert
    shadow.frag
    shadow.vert)

build_shader_files(TARGET point_light FILES ${SHADER_SRC_FILES})
add_custom_target(shaders ALL DEPENDS ${BUILD_SPIRV_FILES})

set(ASSET_FILES
    cornell_box.obj
    cornell_box.mtl)

copy_asset_files(TARGET point_light FILES ${ASSET_FILES})
add_custom_target(assets ALL DEPENDS ${BUILD_ASSET_FILES})