module;

#include <array>
#include <cstdint>
#include <format>
#include <iostream>
#include <optional>
#include <print>
#include <ranges>

#include "GLFW/glfw3.h"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_enums.hpp"

export module nm.engine;

#ifdef NDEBUG
static constexpr bool EnableValidationLayers = false;
#else
static constexpr bool EnableValidationLayers = true;
#endif

namespace nm {

// I know I can inline the `export` modifier but it currently breaks
// tree-sitter :<
export class Engine;

class Engine {
 public:
  struct Config {
    uint32_t width;
    uint32_t height;
    const char* application_name;
  };

  Engine(Config config) : config_(config) {
    InitWindow();
    InitVulkan();
  }

  ~Engine() {
    glfwDestroyWindow(window_);
    glfwTerminate();

    instance_.destroyDebugUtilsMessengerEXT(debug_messenger_, nullptr,
                                            dispatch_loader_);
    device_.destroy();
    instance_.destroy();
  }

  auto Run() -> void { MainLoop(); }

 private:
  static constexpr std::array<const char*, 1> ValidationLayers = {
      "VK_LAYER_KHRONOS_validation"};

  auto MainLoop() -> void {
    while (not glfwWindowShouldClose(window_)) {
      glfwPollEvents();
    }
  }

  auto InitWindow() -> void {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window_ = glfwCreateWindow(config_.width, config_.height, "Vulkan", nullptr,
                               nullptr);
  }

  auto InitVulkan() -> void {
    CreateInstance();
    SetupDebugMessenger();
    PickPhysicalDevice();
    CreateLogicalDevice();
  }

  auto CreateInstance() -> void {
    vk::ApplicationInfo app_info;
    app_info.pApplicationName = config_.application_name;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    const auto required_extensions = GetAndValidateRequiredExtensions();

    vk::InstanceCreateInfo create_info;
    create_info.pApplicationInfo = &app_info;
    create_info.flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
    create_info.enabledExtensionCount =
        static_cast<uint32_t>(required_extensions.size());
    create_info.ppEnabledExtensionNames = required_extensions.data();

    create_info.enabledLayerCount = 0;
    create_info.pNext = nullptr;

    if constexpr (EnableValidationLayers) {
      if (not CheckValidationSupport()) {
        throw std::runtime_error(
            "Validation layers requested but not available");
      }

      std::println("Enabled Validation Layers");

      auto debug_create_info = CreateDebugMessengerCreateInfo();

      create_info.enabledLayerCount =
          static_cast<uint32_t>(ValidationLayers.size());
      create_info.ppEnabledLayerNames = ValidationLayers.data();
      create_info.pNext = &debug_create_info;
    }

    if (vk::createInstance(&create_info, nullptr, &instance_) !=
        vk::Result::eSuccess) {
      throw std::runtime_error("Failed to create a `vk::Instance`");
    }

    dispatch_loader_ =
        vk::detail::DispatchLoaderDynamic(instance_, vkGetInstanceProcAddr);
  }

  auto SetupDebugMessenger() -> void {
    if constexpr (not EnableValidationLayers) {
      return;
    }

    auto create_info = CreateDebugMessengerCreateInfo();

    debug_messenger_ = instance_.createDebugUtilsMessengerEXT(
        create_info, nullptr, dispatch_loader_);
  }

  struct QueueFamilyIndicies {
    std::optional<uint32_t> graphics_family;

    auto IsComplete() const -> bool { return graphics_family.has_value(); }
  };

  auto PickPhysicalDevice() -> void {
    static auto is_device_suitable =
        [](const vk::PhysicalDevice& device) -> bool {
      auto indices = FindQueueFamilies(device);
      return indices.graphics_family.has_value();
    };

    const auto physical_devices = instance_.enumeratePhysicalDevices();
    if (physical_devices.size() == 0) {
      throw std::runtime_error("Failed to find a device with Vulkan support.");
    }

    for (const auto& physical_device : physical_devices) {
      if (is_device_suitable(physical_device)) {
        physical_device_ = physical_device;
        break;
      }
    }
  }

  auto CreateLogicalDevice() -> void {
    const auto indices = FindQueueFamilies(physical_device_);
    vk::DeviceQueueCreateInfo queue_create_info;
    queue_create_info.queueFamilyIndex = *indices.graphics_family;
    queue_create_info.queueCount = 1;
    auto queue_priority = 1.0f;
    queue_create_info.pQueuePriorities = &queue_priority;

    vk::PhysicalDeviceFeatures device_features;
    vk::DeviceCreateInfo device_create_info{};
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = 0;

    if (physical_device_.createDevice(&device_create_info, nullptr, &device_) !=
        vk::Result::eSuccess) {
      throw std::runtime_error("Failed to create logical device.");
    }

    graphics_queue_ =
        device_.getQueue(*indices.graphics_family, /*queueIndex=*/0);
  }

  static auto FindQueueFamilies(const vk::PhysicalDevice& device)
      -> QueueFamilyIndicies {
    QueueFamilyIndicies indices;
    auto queue_families = device.getQueueFamilyProperties();
    auto i = 0;
    for (const auto& queue_family : queue_families) {
      if (queue_family.queueFlags & vk::QueueFlagBits::eGraphics) {
        indices.graphics_family = i;
      }
      if (indices.IsComplete()) {
        break;
      }
      i += 1;
    }
    return indices;
  };

  static auto CheckValidationSupport() -> bool {
    for (const auto& required_layer : ValidationLayers) {
      auto found = false;

      for (const auto& available_layer :
           vk::enumerateInstanceLayerProperties()) {
        if (required_layer == std::string_view(available_layer.layerName)) {
          found = true;
          break;
        }
      }

      if (not found) {
        return false;
      }
    }

    return true;
  }

  // Checks the required vulkan extensions and check if they exists.
  static auto GetAndValidateRequiredExtensions() -> std::vector<const char*> {
    std::vector<const char*> required_extensions;
    auto glfw_extensions_count = 0u;
    auto glfw_extensions =
        glfwGetRequiredInstanceExtensions(&glfw_extensions_count);

    for (auto i : std::views::iota(0u, glfw_extensions_count)) {
      required_extensions.emplace_back(glfw_extensions[i]);
    }

    required_extensions.emplace_back(
        vk::KHRPortabilityEnumerationExtensionName);

    if constexpr (EnableValidationLayers) {
      required_extensions.emplace_back(vk::EXTDebugUtilsExtensionName);
    }

    auto available_extensions = vk::enumerateInstanceExtensionProperties();
    for (const auto& required : required_extensions) {
      bool found = false;
      for (const auto& available : available_extensions) {
        if (required == std::string_view(available.extensionName)) {
          std::println("Found required extension `{}`.", required);
          found = true;
          break;
        }
      }

      if (not found) {
        throw std::runtime_error(std::format(
            "Cannot find the required extension name `{}`.", required));
      }
    }

    return required_extensions;
  }

  static VKAPI_ATTR VkBool32 VKAPI_CALL
  DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
                vk::DebugUtilsMessageTypeFlagsEXT message_type,
                const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
                void* user_data) {
    std::println(std::cerr, "Validation Layer: {}", callback_data->pMessage);
    return VK_FALSE;
  }

  static auto CreateDebugMessengerCreateInfo()
      -> vk::DebugUtilsMessengerCreateInfoEXT {
    vk::DebugUtilsMessengerCreateInfoEXT create_info;

    create_info.messageSeverity =
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;

    create_info.messageType =
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

    create_info.pfnUserCallback = DebugCallback;
    create_info.pUserData = nullptr;

    return create_info;
  }

 private:
  Config config_;
  vk::Instance instance_;
  vk::DebugUtilsMessengerEXT debug_messenger_;
  vk::detail::DispatchLoaderDynamic dispatch_loader_;
  vk::PhysicalDevice physical_device_;
  vk::Device device_;
  vk::Queue graphics_queue_;

  // I know I can use a `std::unique_ptr` but the I don't really want to create
  // a custom deleter :<
  GLFWwindow* window_;
};

}  // namespace nm
