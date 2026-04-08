#include "ui/window_manager.hpp"

#include "Arduino.h"
#include "Enes100.h"
#include "h_main/pin_defns.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

#if defined(MEGA_SIM_HAS_IMGUI_UI)
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {
VkAllocationCallbacks* gAllocator = nullptr;
VkInstance gInstance = VK_NULL_HANDLE;
VkPhysicalDevice gPhysicalDevice = VK_NULL_HANDLE;
VkDevice gDevice = VK_NULL_HANDLE;
uint32_t gQueueFamily = static_cast<uint32_t>(-1);
VkQueue gQueue = VK_NULL_HANDLE;
VkPipelineCache gPipelineCache = VK_NULL_HANDLE;
VkDescriptorPool gDescriptorPool = VK_NULL_HANDLE;
ImGui_ImplVulkanH_Window gMainWindowData;
uint32_t gMinImageCount = 2;
bool gSwapChainRebuild = false;

void checkVkResult(VkResult err)
{
    if (err == VK_SUCCESS) {
        return;
    }
    std::cerr << "[vulkan] VkResult=" << err << std::endl;
    if (err < 0) {
        std::abort();
    }
}

bool isExtensionAvailable(const ImVector<VkExtensionProperties>& properties, const char* extension)
{
    for (const VkExtensionProperties& p : properties) {
        if (std::strcmp(p.extensionName, extension) == 0) {
            return true;
        }
    }
    return false;
}

struct WheelPins {
    const char* label;
    unsigned int enablePin;
    unsigned int dirPin1;
    unsigned int dirPin2;
    ImVec2 offset;
};

constexpr float kNoLoadPwm = 255.0f;
constexpr float kNoLoadMotorSpeed = 19.2f; // replace with your motor's no-load speed in whatever units you prefer

constexpr WheelPins kWheelPins[] = {
    {"FR", FORWARD_RIGHT_MOTOR_ENABLE, FORWARD_RIGHT_MOTOR_DRIVER_1, FORWARD_RIGHT_MOTOR_DRIVER_2, ImVec2(88.0f, -56.0f)},
    {"FL", FORWARD_LEFT_MOTOR_ENABLE, FORWARD_LEFT_MOTOR_DRIVER_1, FORWARD_LEFT_MOTOR_DRIVER_2, ImVec2(-136.0f, -56.0f)},
    {"RR", REAR_RIGHT_MOTOR_ENABLE, REAR_RIGHT_MOTOR_DRIVER_1, REAR_RIGHT_MOTOR_DRIVER_2, ImVec2(88.0f, 56.0f)},
    {"RL", REAR_LEFT_MOTOR_ENABLE, REAR_LEFT_MOTOR_DRIVER_1, REAR_LEFT_MOTOR_DRIVER_2, ImVec2(-136.0f, 56.0f)},
};

float getNormalizedPwm(const ArduinoMega& snapshot, unsigned int pin)
{
    return std::clamp(static_cast<float>(snapshot.pins[pin].value) / kNoLoadPwm, 0.0f, 1.0f) * kNoLoadMotorSpeed;
}

int getWheelDirection(const ArduinoMega& snapshot, const WheelPins& wheel)
{
    const uint8_t d1 = snapshot.pins[wheel.dirPin1].value;
    const uint8_t d2 = snapshot.pins[wheel.dirPin2].value;
    if (d1 == LOW && d2 == HIGH) {
        return 1;
    }
    if (d1 == HIGH && d2 == LOW) {
        return -1;
    }
    return 0;
}

bool wheelIsActive(const ArduinoMega& snapshot, const WheelPins& wheel)
{
    return snapshot.pins[wheel.enablePin].write_pin && snapshot.pins[wheel.enablePin].value > 0;
}

void drawMecanumWheelDisplay(const ArduinoMega& snapshot)
{
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("Mecanum wheel motion");
    ImGui::Text("Enable PWM is scaled by a no-load speed constant of %.2f.", kNoLoadMotorSpeed);

    const ImVec2 canvasStart = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize(460.0f, 320.0f);
    const ImVec2 canvasEnd(canvasStart.x + canvasSize.x, canvasStart.y + canvasSize.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasStart, canvasEnd, IM_COL32(18, 20, 24, 255), 8.0f);
    drawList->AddRect(canvasStart, canvasEnd, IM_COL32(100, 100, 120, 255), 8.0f, 0, 2.0f);

    const ImVec2 center((canvasStart.x + canvasEnd.x) * 0.5f, (canvasStart.y + canvasEnd.y) * 0.5f);
    const ImVec2 chassisMin(center.x - 90.0f, center.y - 55.0f);
    const ImVec2 chassisMax(center.x + 90.0f, center.y + 55.0f);
    drawList->AddRectFilled(chassisMin, chassisMax, IM_COL32(48, 54, 66, 255), 10.0f);
    drawList->AddRect(chassisMin, chassisMax, IM_COL32(210, 210, 220, 255), 10.0f, 0, 2.0f);
    drawList->AddText(ImVec2(center.x - 18.0f, center.y - 8.0f), IM_COL32(240, 240, 240, 255), "OTV");

    const float wheelW = 54.0f;
    const float wheelH = 30.0f;

    for (const WheelPins& wheelPins : kWheelPins) {
        const ImVec2 wheelCenter(center.x + wheelPins.offset.x, center.y + wheelPins.offset.y);
        const ImVec2 wheelMin(wheelCenter.x - wheelW * 0.5f, wheelCenter.y - wheelH * 0.5f);
        const ImVec2 wheelMax(wheelCenter.x + wheelW * 0.5f, wheelCenter.y + wheelH * 0.5f);

        const bool active = wheelIsActive(snapshot, wheelPins);
        const int direction = getWheelDirection(snapshot, wheelPins);
        const float speed = getNormalizedPwm(snapshot, wheelPins.enablePin);
        const float speedRatio = std::clamp(speed / kNoLoadMotorSpeed, 0.0f, 1.0f);

        ImU32 fill = IM_COL32(70, 70, 74, 255);
        if (active) {
            fill = direction >= 0 ? IM_COL32(65, 155, 95, 255) : IM_COL32(190, 80, 75, 255);
        }

        drawList->AddRectFilled(wheelMin, wheelMax, fill, 8.0f);
        drawList->AddRect(wheelMin, wheelMax, IM_COL32(235, 235, 240, 255), 8.0f, 0, 1.5f);
        drawList->AddText(ImVec2(wheelMin.x + 6.0f, wheelMin.y + 5.0f), IM_COL32(255, 255, 255, 255), wheelPins.label);

        if (active && direction != 0) {
            const float arrowLen = 16.0f + 24.0f * speedRatio;
            const ImVec2 arrowStart(wheelCenter.x, wheelCenter.y);
            const ImVec2 arrowEnd(wheelCenter.x, wheelCenter.y - arrowLen * static_cast<float>(direction));
            drawList->AddLine(arrowStart, arrowEnd, IM_COL32(255, 255, 255, 255), 3.0f);
            drawList->AddTriangleFilled(
                ImVec2(arrowEnd.x - 6.0f, arrowEnd.y + (direction > 0 ? 8.0f : -8.0f)),
                ImVec2(arrowEnd.x + 6.0f, arrowEnd.y + (direction > 0 ? 8.0f : -8.0f)),
                ImVec2(arrowEnd.x, arrowEnd.y),
                IM_COL32(255, 255, 255, 255));
        } else {
            drawList->AddLine(ImVec2(wheelCenter.x - 12.0f, wheelCenter.y), ImVec2(wheelCenter.x + 12.0f, wheelCenter.y), IM_COL32(190, 190, 190, 255), 2.0f);
        }

        const ImVec2 barMin(wheelMin.x, wheelMax.y + 6.0f);
        const ImVec2 barMax(wheelMax.x, wheelMax.y + 16.0f);
        drawList->AddRectFilled(barMin, barMax, IM_COL32(45, 45, 48, 255), 4.0f);
        drawList->AddRectFilled(barMin, ImVec2(barMin.x + (barMax.x - barMin.x) * speedRatio, barMax.y), IM_COL32(110, 180, 240, 255), 4.0f);

        ImGui::SetCursorScreenPos(ImVec2(wheelMin.x, barMax.y + 8.0f));
        ImGui::Text("%s: %s %+.2f", wheelPins.label, active ? "active" : "stopped", active ? (direction >= 0 ? speed : -speed) : 0.0f);
    }

    ImGui::Dummy(ImVec2(canvasSize.x, canvasSize.y + 26.0f));
}

void setupVulkan(ImVector<const char*> instanceExtensions)
{
    VkResult err;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    uint32_t propertyCount = 0;
    ImVector<VkExtensionProperties> properties;
    vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, nullptr);
    properties.resize(propertyCount);
    err = vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, properties.Data);
    checkVkResult(err);

    if (isExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
        instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    if (isExtensionAvailable(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif

    createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.Size);
    createInfo.ppEnabledExtensionNames = instanceExtensions.Data;
    err = vkCreateInstance(&createInfo, gAllocator, &gInstance);
    checkVkResult(err);

    gPhysicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(gInstance);
    gQueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(gPhysicalDevice);

    ImVector<const char*> deviceExtensions;
    deviceExtensions.push_back("VK_KHR_swapchain");

    uint32_t deviceExtensionCount = 0;
    ImVector<VkExtensionProperties> deviceProperties;
    vkEnumerateDeviceExtensionProperties(gPhysicalDevice, nullptr, &deviceExtensionCount, nullptr);
    deviceProperties.resize(deviceExtensionCount);
    vkEnumerateDeviceExtensionProperties(gPhysicalDevice, nullptr, &deviceExtensionCount, deviceProperties.Data);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    if (isExtensionAvailable(deviceProperties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
        deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    }
#endif

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = gQueueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.Size);
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.Data;
    err = vkCreateDevice(gPhysicalDevice, &deviceCreateInfo, gAllocator, &gDevice);
    checkVkResult(err);
    vkGetDeviceQueue(gDevice, gQueueFamily, 0, &gQueue);

    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE},
    };
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 0;
    for (VkDescriptorPoolSize& poolSize : poolSizes) {
        poolInfo.maxSets += poolSize.descriptorCount;
    }
    poolInfo.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(poolSizes));
    poolInfo.pPoolSizes = poolSizes;
    err = vkCreateDescriptorPool(gDevice, &poolInfo, gAllocator, &gDescriptorPool);
    checkVkResult(err);
}

void setupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
{
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(gPhysicalDevice, gQueueFamily, surface, &res);
    if (res != VK_TRUE) {
        std::cerr << "No WSI support found for selected GPU" << std::endl;
        std::abort();
    }

    const VkFormat requestSurfaceImageFormat[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8_UNORM,
        VK_FORMAT_R8G8B8_UNORM,
    };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->Surface = surface;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        gPhysicalDevice,
        wd->Surface,
        requestSurfaceImageFormat,
        IM_ARRAYSIZE(requestSurfaceImageFormat),
        requestSurfaceColorSpace);

    VkPresentModeKHR presentModes[] = {VK_PRESENT_MODE_FIFO_KHR};
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(gPhysicalDevice, wd->Surface, presentModes, IM_ARRAYSIZE(presentModes));

    ImGui_ImplVulkanH_CreateOrResizeWindow(
        gInstance,
        gPhysicalDevice,
        gDevice,
        wd,
        gQueueFamily,
        gAllocator,
        width,
        height,
        gMinImageCount,
        0);
}

void cleanupVulkanWindow(ImGui_ImplVulkanH_Window* wd)
{
    ImGui_ImplVulkanH_DestroyWindow(gInstance, gDevice, wd, gAllocator);
    vkDestroySurfaceKHR(gInstance, wd->Surface, gAllocator);
}

void cleanupVulkan()
{
    vkDestroyDescriptorPool(gDevice, gDescriptorPool, gAllocator);
    vkDestroyDevice(gDevice, gAllocator);
    vkDestroyInstance(gInstance, gAllocator);
}

void frameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* drawData)
{
    VkSemaphore imageAcquiredSemaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore renderCompleteSemaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkResult err = vkAcquireNextImageKHR(gDevice, wd->Swapchain, UINT64_MAX, imageAcquiredSemaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        gSwapChainRebuild = true;
    }
    if (err == VK_ERROR_OUT_OF_DATE_KHR) {
        return;
    }
    if (err != VK_SUBOPTIMAL_KHR) {
        checkVkResult(err);
    }

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];

    err = vkWaitForFences(gDevice, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
    checkVkResult(err);
    err = vkResetFences(gDevice, 1, &fd->Fence);
    checkVkResult(err);

    err = vkResetCommandPool(gDevice, fd->CommandPool, 0);
    checkVkResult(err);
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(fd->CommandBuffer, &beginInfo);
    checkVkResult(err);

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = wd->RenderPass;
    renderPassInfo.framebuffer = fd->Framebuffer;
    renderPassInfo.renderArea.extent.width = wd->Width;
    renderPassInfo.renderArea.extent.height = wd->Height;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &wd->ClearValue;
    vkCmdBeginRenderPass(fd->CommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    ImGui_ImplVulkan_RenderDrawData(drawData, fd->CommandBuffer);

    vkCmdEndRenderPass(fd->CommandBuffer);
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAcquiredSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &fd->CommandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderCompleteSemaphore;

    err = vkEndCommandBuffer(fd->CommandBuffer);
    checkVkResult(err);
    err = vkQueueSubmit(gQueue, 1, &submitInfo, fd->Fence);
    checkVkResult(err);
}

void framePresent(ImGui_ImplVulkanH_Window* wd)
{
    if (gSwapChainRebuild) {
        return;
    }

    VkSemaphore renderCompleteSemaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderCompleteSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &wd->Swapchain;
    presentInfo.pImageIndices = &wd->FrameIndex;

    VkResult err = vkQueuePresentKHR(gQueue, &presentInfo);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        gSwapChainRebuild = true;
    }
    if (err == VK_ERROR_OUT_OF_DATE_KHR) {
        return;
    }
    if (err != VK_SUBOPTIMAL_KHR) {
        checkVkResult(err);
    }

    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
}

void drawPinsUi()
{
    const ArduinoMega snapshot = getArduinoSnapshot();

    ImGui::Begin("Pin Monitor");
    ImGui::Text("Digital pins");
    if (ImGui::BeginTable("digital_pins", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY, ImVec2(0, 300))) {
        ImGui::TableSetupColumn("Pin");
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupColumn("Mode");
        ImGui::TableHeadersRow();
        for (int i = 0; i < 54; ++i) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", i);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", snapshot.pins[i].value);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", snapshot.pins[i].write_pin ? "OUTPUT" : "INPUT");
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::Text("Analog inputs");
    if (ImGui::BeginTable("analog_pins", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Analog Pin");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();
        for (int i = 0; i < 16; ++i) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("A%d", i);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", snapshot.analogInputs[i]);
        }
        ImGui::EndTable();
    }
    drawMecanumWheelDisplay(snapshot);
    Enes100.updatePoseEstimate();
    ImGui::Text("Estimated pose (m, rad)");
    ImGui::Text("x: %+.2f", Enes100Class::getX());
    ImGui::Text("y: %+.2f", Enes100Class::getY());
    ImGui::Text("theta: %+.3f", Enes100Class::getTheta());
    const ArduinoMega updatedSnapshot = getArduinoSnapshot();
    ImGui::Text("arduino.theta: %+.3f", updatedSnapshot.theta);
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::End();
}
} // namespace
#endif

namespace ui {
namespace {
std::thread gWindowThread;
std::atomic<bool> gStopRequested{false};
std::atomic<bool> gRunning{false};
WindowManagerConfig gConfig;

void glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        gStopRequested.store(true);
    }
}

void runWindowThread()
{
#if defined(MEGA_SIM_HAS_IMGUI_UI)
    glfwSetErrorCallback([](int error, const char* description) {
        std::cerr << "GLFW error " << error << ": " << description << std::endl;
    });

    if (!glfwInit()) {
        gStopRequested.store(true);
        gRunning.store(false);
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(gConfig.width, gConfig.height, gConfig.title, nullptr, nullptr);
    if (window == nullptr || !glfwVulkanSupported()) {
        std::cerr << "GLFW window/Vulkan setup failed. UI disabled." << std::endl;
        if (window != nullptr) {
            glfwDestroyWindow(window);
        }
        glfwTerminate();
        gStopRequested.store(true);
        gRunning.store(false);
        arduino.running = false;
        return;
    }

    ImVector<const char*> extensions;
    uint32_t extensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    for (uint32_t i = 0; i < extensionCount; ++i) {
        extensions.push_back(glfwExtensions[i]);
    }

    setupVulkan(extensions);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult err = glfwCreateWindowSurface(gInstance, window, gAllocator, &surface);
    checkVkResult(err);

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    ImGui_ImplVulkanH_Window* wd = &gMainWindowData;
    setupVulkanWindow(wd, surface, width, height);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    glfwSetKeyCallback(window, glfwKeyCallback);
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = gInstance;
    initInfo.PhysicalDevice = gPhysicalDevice;
    initInfo.Device = gDevice;
    initInfo.QueueFamily = gQueueFamily;
    initInfo.Queue = gQueue;
    initInfo.PipelineCache = gPipelineCache;
    initInfo.DescriptorPool = gDescriptorPool;
    initInfo.MinImageCount = gMinImageCount;
    initInfo.ImageCount = wd->ImageCount;
    initInfo.Allocator = gAllocator;
    initInfo.PipelineInfoMain.RenderPass = wd->RenderPass;
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.CheckVkResultFn = checkVkResult;
    ImGui_ImplVulkan_Init(&initInfo);

    while (!gStopRequested.load() && !glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int fbWidth = 0;
        int fbHeight = 0;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        if (fbWidth > 0 && fbHeight > 0 && (gSwapChainRebuild || wd->Width != fbWidth || wd->Height != fbHeight)) {
            ImGui_ImplVulkan_SetMinImageCount(gMinImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(gInstance, gPhysicalDevice, gDevice, wd, gQueueFamily, gAllocator, fbWidth, fbHeight, gMinImageCount, 0);
            wd->FrameIndex = 0;
            gSwapChainRebuild = false;
        }

        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            requestWindowManagerStop();
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        drawPinsUi();

        ImGui::Render();
        ImDrawData* mainDrawData = ImGui::GetDrawData();
        const bool isMinimized = (mainDrawData->DisplaySize.x <= 0.0f || mainDrawData->DisplaySize.y <= 0.0f);
        wd->ClearValue.color.float32[0] = 0.08f;
        wd->ClearValue.color.float32[1] = 0.08f;
        wd->ClearValue.color.float32[2] = 0.10f;
        wd->ClearValue.color.float32[3] = 1.0f;

        if (!isMinimized) {
            frameRender(wd, mainDrawData);
            framePresent(wd);
        }
    }

    gStopRequested.store(true);

    err = vkDeviceWaitIdle(gDevice);
    checkVkResult(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    cleanupVulkanWindow(wd);
    cleanupVulkan();
    glfwDestroyWindow(window);
    glfwTerminate();
#else
    while (!gStopRequested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
#endif

    gRunning.store(false);
}
} // namespace

bool startWindowManager(const WindowManagerConfig& config)
{
    if (gRunning.load()) {
        return false;
    }

    gConfig = config;
    gStopRequested.store(false);
    gRunning.store(true);
    gWindowThread = std::thread(runWindowThread);
    return true;
}

void requestWindowManagerStop()
{
    gStopRequested.store(true);
    arduino.running = false;
}

bool isWindowManagerStopRequested()
{
    return gStopRequested.load();
}

void joinWindowManager()
{
    if (gWindowThread.joinable()) {
        gWindowThread.join();
    }
}

} // namespace ui


