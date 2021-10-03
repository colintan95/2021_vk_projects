cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

project(vk_projects)

set(GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw")
add_subdirectory(${GLFW_DIR})

find_package(Vulkan REQUIRED FATAL_ERROR)

add_subdirectory(hello_triangle)
