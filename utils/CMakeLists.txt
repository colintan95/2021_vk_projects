add_library(utils
    camera.cpp
    camera.h
    model.cpp
    model.h
    vk.cpp
    vk.h)

target_include_directories(utils PRIVATE "${PROJECT_ROOT_DIR}")

target_include_directories(utils PUBLIC ${GLM_INCLUDE_DIR})

target_link_libraries(utils PRIVATE Vulkan::Vulkan)