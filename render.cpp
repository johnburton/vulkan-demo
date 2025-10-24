#include "render.h"

#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

static GLFWwindow* window;
static VkInstance instance;
static VkPhysicalDevice physical_device;
static VkSurfaceKHR surface;
static VkDevice device;
static VkQueue graphics_queue;
static uint32_t graphics_queue_family_index;
static VkSwapchainKHR swapchain;

static VkCommandPool command_pool;
static VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];

static VkFence image_available_fences[MAX_FRAMES_IN_FLIGHT];

static VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
static VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];

static uint32_t current_frame = 0;
static uint32_t current_image_index = 0;

static void init_window() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(800, 600, "Vulkan Window", nullptr, nullptr);
}

static void init_vulkan_instance() {

    // Print the vulkan library version
    uint32_t api_version;
    vkEnumerateInstanceVersion(&api_version);
    uint32_t major = VK_VERSION_MAJOR(api_version);
    uint32_t minor = VK_VERSION_MINOR(api_version);
    uint32_t patch = VK_VERSION_PATCH(api_version);
    printf("• Vulkan library version: %d.%d.%d\n", major, minor, patch);

    const char* required_layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    uint32_t extension_count = 0;
    const char** required_extensions = glfwGetRequiredInstanceExtensions(&extension_count);

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = required_layers;
    createInfo.enabledExtensionCount = extension_count;
    createInfo.ppEnabledExtensionNames = required_extensions;

    vkCreateInstance(&createInfo, nullptr, &instance);
    printf("• Vulkan instance created.\n");
}

void init_vulkan_physical_device() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    if (device_count == 0) {
        fprintf(stderr, "🔸Failed to find GPUs with Vulkan support!\n");
        exit(EXIT_FAILURE);
    }

    VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices);

    physical_device = devices[0];
    free(devices);

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physical_device, &deviceProperties);
    printf("• Selected GPU: %s\n", deviceProperties.deviceName);
}

static void init_vulkan_surface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        fprintf(stderr, "🔸Failed to create window surface\n");
        exit(EXIT_FAILURE);
    }
    printf("• Vulkan surface created.\n");

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surfaceCapabilities);
    printf("  - Min image count: %d\n", surfaceCapabilities.minImageCount);
    printf("  - Max image count: %d\n", surfaceCapabilities.maxImageCount);
    printf("  - Current extent: %d x %d\n", surfaceCapabilities.currentExtent.width, surfaceCapabilities.currentExtent.height);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &formatCount, nullptr);
    VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR) * formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &formatCount, formats);
    printf("  - Supported formats:\n");
    for (uint32_t i = 0; i < formatCount; i++) {
        printf("    • Format ID: %d, Color Space ID: %d\n", formats[i].format, formats[i].colorSpace);
    }
    free(formats);
}

static void init_vulkan_device() {

    const char* device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // Find a suitable queue family
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queueFamilies);

    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_queue_family_index = i;
            break;
        }
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphics_queue_family_index;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = device_extensions;

    vkCreateDevice(physical_device, &createInfo, nullptr, &device);

    vkGetDeviceQueue(device, 0, 0, &graphics_queue);

    printf("• Logical device created.\n");
}

void init_vulkan_swapchain() {

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surfaceCapabilities);

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = surfaceCapabilities.minImageCount;
    create_info.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    create_info.imageExtent = {800, 600};
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    create_info.clipped = VK_TRUE;

    vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain);

    printf("• Swapchain created.\n");
}

static void init_vulkan_command_buffers() {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = graphics_queue_family_index;

    vkCreateCommandPool(device, &pool_info, nullptr, &command_pool);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    vkAllocateCommandBuffers(device, &alloc_info, command_buffers);

    printf("• Command buffers allocated.\n");
}

static void init_vulkan_sync_objects() {
    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // start signaled

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkCreateSemaphore(device, &semInfo, nullptr, &image_available_semaphores[i]);
        vkCreateSemaphore(device, &semInfo, nullptr, &render_finished_semaphores[i]);
        vkCreateFence(device, &fenceInfo, nullptr, &image_available_fences[i]);
    }

    printf("• Synchronization objects created.\n");
}

void render_init() {
    init_window();
    init_vulkan_instance();
    init_vulkan_physical_device();
    init_vulkan_device();
    init_vulkan_surface();
    init_vulkan_swapchain();
    init_vulkan_command_buffers();
    init_vulkan_sync_objects();
}

bool render_should_close() {
    glfwPollEvents();
    return glfwWindowShouldClose(window);
}