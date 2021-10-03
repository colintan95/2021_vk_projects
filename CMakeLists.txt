cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 11)

project(vk_projects)

set(GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw")

add_subdirectory(${GLFW_DIR})

add_executable(hello_triangle hello_triangle/main.cpp hello_triangle/app.h hello_triangle/app.cpp)

# target_include_directories(hello_triangle PRIVATE "${GLFW_DIR}/include")
target_link_libraries(hello_triangle PRIVATE glfw)

find_package(Vulkan REQUIRED FATAL_ERROR)

target_link_libraries(hello_triangle PRIVATE Vulkan::Vulkan)