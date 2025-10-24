#include "render.h"

#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct Shader {
    VkShaderModule frag_module;
    VkShaderModule vert_module;
};

struct Material {
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
};

// NOTE: We use dynamic rendering everywhere possible, so no render passes or framebuffers are created.

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

static uint32_t swapchain_image_count = 0;
static VkImage* swapchain_images;
static VkImageView* swapchain_image_views;

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
    printf("  - Min image count: %u\n", surfaceCapabilities.minImageCount);
    printf("  - Max image count: %u\n", surfaceCapabilities.maxImageCount);
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
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
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

    // Enable dynamic rendering feature (required for vkCmdBeginRendering/vkCmdEndRendering)
    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_feats{};
    dynamic_rendering_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamic_rendering_feats.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = device_extensions;
    createInfo.pNext = &dynamic_rendering_feats;

    vkCreateDevice(physical_device, &createInfo, nullptr, &device);

    vkGetDeviceQueue(device, graphics_queue_family_index, 0, &graphics_queue);

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

    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, nullptr);
    swapchain_images = (VkImage*)malloc(sizeof(VkImage) * swapchain_image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_images);

    swapchain_image_views = (VkImageView*)malloc(sizeof(VkImageView) * swapchain_image_count);
    for (uint32_t i = 0; i < swapchain_image_count; ++i) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = swapchain_images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_B8G8R8A8_SRGB;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        vkCreateImageView(device, &view_info, nullptr, &swapchain_image_views[i]);
    }


    printf("• Swapchain created.\n");
}

static void init_vulkan_command_buffers() {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
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

void render_begin_frame() {
    vkWaitForFences(device, 1, &image_available_fences[current_frame], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &image_available_fences[current_frame]);

    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, image_available_semaphores[current_frame], VK_NULL_HANDLE, &current_image_index);

    VkCommandBuffer command_buffer = command_buffers[current_frame];

    vkResetCommandBuffer(command_buffer, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkBeginCommandBuffer(command_buffer, &begin_info);
  // Transition swapchain image from UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL before rendering.
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = swapchain_images[current_image_index];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    // Begin dynamic rendering and clear the color attachment.
    VkRenderingAttachmentInfo color_attachment{};
    color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachment.imageView = swapchain_image_views[current_image_index];
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.clearValue.color = { {0.1f, 0.0f, 0.0f, 1.0f} }; // clear to black (RGBA)

    VkRenderingInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea.offset = {0, 0};
    rendering_info.renderArea.extent = {800, 600};
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;

    vkCmdBeginRendering(command_buffer, &rendering_info);

    printf("• Frame %d image index %d: Command buffer recording started.\n", current_frame, current_image_index);
}

void render_end_frame() {
    VkCommandBuffer command_buffer = command_buffers[current_frame];

    vkCmdEndRendering(command_buffer);

      // Transition the swapchain image to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR before presenting.
    // Record an image memory barrier into the current command buffer.
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // assumed previous layout
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = swapchain_images[current_image_index];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = 0;

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = { image_available_semaphores[current_frame] };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    VkSemaphore signal_semaphores[] = { render_finished_semaphores[current_frame] };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    vkQueueSubmit(graphics_queue, 1, &submit_info, image_available_fences[current_frame]);

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swapchains[] = { swapchain };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &current_image_index;

    vkQueuePresentKHR(graphics_queue, &present_info);

    printf("• Frame %d presented.\n", current_frame);

    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool render_should_close() {
    glfwPollEvents();
    return glfwWindowShouldClose(window);
}

void render_create_shader(Shader **shader, Shader_Data *shader_data) {
    *shader = new Shader();

    VkShaderModuleCreateInfo vert_create_info{};
    vert_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vert_create_info.codeSize = shader_data->vert_size;
    vert_create_info.pCode = (const uint32_t*)shader_data->vert_source;

    vkCreateShaderModule(device, &vert_create_info, nullptr, &(*shader)->vert_module);
    VkShaderModuleCreateInfo frag_create_info{};
    frag_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    frag_create_info.codeSize = shader_data->frag_size;
    frag_create_info.pCode = (const uint32_t*)shader_data->frag_source;

    vkCreateShaderModule(device, &frag_create_info, nullptr, &(*shader)->frag_module);
}

void render_destroy_shader(Shader *shader) {
    vkDestroyShaderModule(device, shader->vert_module, nullptr);
    vkDestroyShaderModule(device, shader->frag_module, nullptr);
    delete shader;
}

void render_create_material(Material **material, Shader *shader) {
    *material = new Material();

    // Create empty pipeline layout (no descriptor sets or push constants for now)
    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 0;
    layout_info.pSetLayouts = nullptr;
    layout_info.pushConstantRangeCount = 0;
    layout_info.pPushConstantRanges = nullptr;

    vkCreatePipelineLayout(device, &layout_info, nullptr, &(*material)->pipeline_layout);

    // Shader stages
    VkPipelineShaderStageCreateInfo vert_stage{};
    vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage.module = shader->vert_module;
    vert_stage.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage{};
    frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage.module = shader->frag_module;
    frag_stage.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[2] = { vert_stage, frag_stage };

    // No vertex input (we'll use vertex id in shader)
    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 0;
    vertex_input.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.depthClampEnable = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_FALSE;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    raster.depthBiasEnable = VK_FALSE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_attachment{};
    color_attachment.blendEnable = VK_FALSE;
    color_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend{};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &color_attachment;

    VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamic_states;

    // Inform pipeline about dynamic rendering and the color format we will render into.
    VkFormat color_format = VK_FORMAT_B8G8R8A8_SRGB;
    VkPipelineRenderingCreateInfo pipeline_rendering{};
    pipeline_rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipeline_rendering.viewMask = 0;
    pipeline_rendering.colorAttachmentCount = 1;
    pipeline_rendering.pColorAttachmentFormats = &color_format;

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = &pipeline_rendering;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &raster;
    pipeline_info.pMultisampleState = &ms;
    pipeline_info.pColorBlendState = &color_blend;
    pipeline_info.pDynamicState = &dynamic;
    pipeline_info.layout = (*material)->pipeline_layout;

    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &(*material)->pipeline);
}

void render_destroy_material(Material *material) {
    vkDestroyPipeline(device, material->pipeline, nullptr);
    vkDestroyPipelineLayout(device, material->pipeline_layout, nullptr);
    delete material;
}

void render_draw(Material* material) {
    VkCommandBuffer command_buffer = command_buffers[current_frame];

    // Bind the graphics pipeline
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material->pipeline);

    // Set dynamic viewport and scissor to match the rendering area (800x600)
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 600.0f;
    viewport.width = 800.0f;
    viewport.height = -600.0f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {800, 600};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    // Issue a simple draw call (3 vertices -> triangle)
    vkCmdDraw(command_buffer, 3, 1, 0, 0);
}

void render_wait_idle() {
    vkDeviceWaitIdle(device);
}