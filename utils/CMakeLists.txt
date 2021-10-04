add_library(utils
    model.cpp
    model.h)

target_include_directories(utils PUBLIC ${GLM_INCLUDE_DIR})