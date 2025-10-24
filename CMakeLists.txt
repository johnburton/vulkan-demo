cmake_minimum_required(VERSION 3.20)
project(game VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(libs/glfw)
find_package(Vulkan REQUIRED)

add_executable(main main.cpp render.cpp assets.cpp)
target_link_libraries(main PRIVATE glfw Vulkan::Vulkan)

# Simple custom command: compile GLSL shaders in source `assets/shaders` into
# the build output `assets/shaders` as .spv before building `main`.
add_custom_command(TARGET main PRE_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/assets/shaders
	COMMAND glslangValidator -V ${CMAKE_SOURCE_DIR}/assets/shaders/triangle.vert.glsl -o ${CMAKE_BINARY_DIR}/assets/shaders/triangle.vert.spv
	COMMAND glslangValidator -V ${CMAKE_SOURCE_DIR}/assets/shaders/triangle.frag.glsl -o ${CMAKE_BINARY_DIR}/assets/shaders/triangle.frag.spv
	COMMENT "Compiling triangle shaders to ${CMAKE_BINARY_DIR}/assets/shaders"
)