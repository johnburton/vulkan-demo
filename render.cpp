#include "render.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

static GLFWwindow* window;

static void init_window() {
    glfwInit();
    window = glfwCreateWindow(800, 600, "Vulkan Window", nullptr, nullptr);
}

void render_init() {
    init_window();
}

bool render_should_close() {
    glfwPollEvents();
    return glfwWindowShouldClose(window);
}