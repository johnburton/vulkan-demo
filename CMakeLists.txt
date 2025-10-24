cmake_minimum_required(VERSION 3.20)
project(game VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(libs/glfw)
find_package(Vulkan REQUIRED)

add_executable(main main.cpp render.cpp)
target_link_libraries(main PRIVATE glfw Vulkan::Vulkan)