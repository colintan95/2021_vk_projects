add_library(utils
    camera.cpp
    camera.h
    model.cpp
    model.h)

target_include_directories(utils PRIVATE "${PROJECT_ROOT_DIR}")

target_include_directories(utils PUBLIC ${GLM_INCLUDE_DIR})