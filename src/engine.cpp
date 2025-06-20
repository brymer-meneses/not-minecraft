module;

#include <array>
#include <cstdint>
#include <format>
#include <iostream>
#include <optional>
#include <print>
#include <ranges>
#include <set>

#define GLFW_INCLUDE_VULKAN
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

    if constexpr (EnableValidationLayers) {
      instance_.destroyDebugUtilsMessengerEXT(debug_messenger_, nullptr,
                                              dispatch_loader_);
    }

    for (auto image_view : swap_chain_image_views_) {
      device_.destroyImageView(image_view);
    }

    device_.destroySwapchainKHR(swap_chain_);
    instance_.destroySurfaceKHR(surface_);
    device_.destroy();
    instance_.destroy();
  }

  auto Run() -> void { MainLoop(); }

 private:
  static constexpr std::array<const char*, 1> ValidationLayers = {
      "VK_LAYER_KHRONOS_validation"};
  static constexpr std::array<const char*, 1> DeviceExtensions = {
      vk::KHRSwapchainExtensionName,
  };

  struct QueueFamilyIndicies {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;

    auto IsComplete() const -> bool {
      return graphics_family.has_value() and present_family.has_value();
    }
  };

  struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> present_modes;
  };

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
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapChain();
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

  auto CreateSurface() -> void {
    if (glfwCreateWindowSurface(instance_, window_, nullptr,
                                reinterpret_cast<VkSurfaceKHR*>(&surface_)) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create window surface.");
    }

    std::cout << "SURFACE: " << surface_ << "\n";

    assert(surface_ != VK_NULL_HANDLE);
  }

  auto PickPhysicalDevice() -> void {
    auto is_device_suitable = [this](const vk::PhysicalDevice& device) -> bool {
      const auto indices = FindQueueFamilies(device, surface_);
      const auto is_extensions_supported = CheckDeviceExtensionSupport(device);

      auto swap_chain_adequate = false;
      if (is_extensions_supported) {
        auto support_details = QuerySwapChainSupport(device, surface_);
        swap_chain_adequate = not support_details.formats.empty() and
                              not support_details.present_modes.empty();
      }
      return indices.IsComplete() and is_extensions_supported and
             swap_chain_adequate;
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

    if (physical_device_ == VK_NULL_HANDLE) {
      throw std::runtime_error("Failed to find a suitable GPU.");
    }
  }

  auto CreateLogicalDevice() -> void {
    const auto indices = FindQueueFamilies(physical_device_, surface_);
    assert(indices.IsComplete());

    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    std::set unique_queue_families = {*indices.graphics_family,
                                      *indices.present_family};

    auto queue_priority = 1.0f;
    for (const auto queue_family : unique_queue_families) {
      vk::DeviceQueueCreateInfo queue_create_info;
      queue_create_info.queueFamilyIndex = queue_family;
      queue_create_info.queueCount = 1;
      queue_create_info.pQueuePriorities = &queue_priority;
      queue_create_infos.push_back(queue_create_info);
    }

    vk::PhysicalDeviceFeatures device_features;
    vk::DeviceCreateInfo device_create_info{};
    device_create_info.queueCreateInfoCount =
        static_cast<uint32_t>(queue_create_infos.size());
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount =
        static_cast<uint32_t>(DeviceExtensions.size());
    device_create_info.ppEnabledExtensionNames = DeviceExtensions.data();

    if (physical_device_.createDevice(&device_create_info, nullptr, &device_) !=
        vk::Result::eSuccess) {
      throw std::runtime_error("Failed to create logical device.");
    }

    graphics_queue_ =
        device_.getQueue(*indices.graphics_family, /*queueIndex=*/0);
    present_queue_ =
        device_.getQueue(*indices.present_family, /*queueIndex=*/0);
  }

  auto CreateSwapChain() -> void {
    auto swap_chain_support = QuerySwapChainSupport(physical_device_, surface_);

    auto surface_format = ChooseSwapSurfaceFormat(swap_chain_support.formats);
    auto present_mode = ChooseSwapPresentMode(swap_chain_support.present_modes);
    auto extent = ChooseSwapExtent(swap_chain_support.capabilities, window_);

    auto image_count = swap_chain_support.capabilities.minImageCount + 1;
    if (swap_chain_support.capabilities.maxImageCount > 0 and
        image_count > swap_chain_support.capabilities.maxImageCount) {
      image_count = swap_chain_support.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR create_info;
    create_info.surface = surface_;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    auto indices = FindQueueFamilies(physical_device_, surface_);
    uint32_t queue_family_indices[] = {*indices.graphics_family,
                                       *indices.present_family};
    if (indices.graphics_family != indices.present_family) {
      create_info.imageSharingMode = vk::SharingMode::eConcurrent;
      create_info.queueFamilyIndexCount = 2;
      create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
      create_info.imageSharingMode = vk::SharingMode::eExclusive;
      create_info.queueFamilyIndexCount = 0;
      create_info.pQueueFamilyIndices = nullptr;
    }

    // We do not want any transformations.
    create_info.preTransform = swap_chain_support.capabilities.currentTransform;
    create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    swap_chain_ = device_.createSwapchainKHR(create_info, nullptr);
    swap_chain_images_ = device_.getSwapchainImagesKHR(swap_chain_);
    swap_chain_image_format_ = surface_format.format;
    swap_chain_extent_ = extent;
  }

  auto CreateImageViews() -> void {
    swap_chain_image_views_.reserve(swap_chain_images_.size());

    for (const auto& image : swap_chain_images_) {
      vk::ImageViewCreateInfo create_info;
      create_info.image = image;
      create_info.viewType = vk::ImageViewType::e2D;
      create_info.format = swap_chain_image_format_;

      create_info.components.r = vk::ComponentSwizzle::eIdentity;
      create_info.components.g = vk::ComponentSwizzle::eIdentity;
      create_info.components.b = vk::ComponentSwizzle::eIdentity;
      create_info.components.a = vk::ComponentSwizzle::eIdentity;

      create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
      create_info.subresourceRange.baseMipLevel = 0;
      create_info.subresourceRange.levelCount = 1;
      create_info.subresourceRange.baseArrayLayer = 0;
      create_info.subresourceRange.layerCount = 0;

      swap_chain_image_views_.emplace_back(
          device_.createImageView(create_info, nullptr));
    }
  }

  static auto ChooseSwapSurfaceFormat(
      std::span<vk::SurfaceFormatKHR> available_formats)
      -> vk::SurfaceFormatKHR {
    for (const auto& format : available_formats) {
      if (format.format == vk::Format::eB8G8R8Srgb &&
          format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
        return format;
      }
    }
    return available_formats[0];
  }

  static auto ChooseSwapPresentMode(
      std::span<vk::PresentModeKHR> available_present_modes)
      -> vk::PresentModeKHR {
    for (const auto& present_mode : available_present_modes) {
      if (present_mode == vk::PresentModeKHR::eMailbox) {
        return present_mode;
      }
    }
    return vk::PresentModeKHR::eFifo;
  }

  static auto ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities,
                               GLFWwindow* window) -> vk::Extent2D {
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    }
    int32_t width = 0;
    int32_t height = 0;

    glfwGetFramebufferSize(window, &width, &height);

    vk::Extent2D actual_extent;
    actual_extent.width = std::clamp(static_cast<uint32_t>(width),
                                     capabilities.minImageExtent.width,
                                     capabilities.maxImageExtent.width);
    actual_extent.height = std::clamp(static_cast<uint32_t>(height),
                                      capabilities.minImageExtent.width,
                                      capabilities.maxImageExtent.width);
    return actual_extent;
  }

  static auto QuerySwapChainSupport(vk::PhysicalDevice device,
                                    vk::SurfaceKHR surface)
      -> SwapChainSupportDetails {
    assert(surface != VK_NULL_HANDLE);

    SwapChainSupportDetails swap_chain_support;

    swap_chain_support.capabilities = device.getSurfaceCapabilitiesKHR(surface);
    swap_chain_support.present_modes =
        device.getSurfacePresentModesKHR(surface);
    swap_chain_support.formats = device.getSurfaceFormatsKHR(surface);
    return swap_chain_support;
  }

  static auto FindQueueFamilies(vk::PhysicalDevice device,
                                vk::SurfaceKHR surface) -> QueueFamilyIndicies {
    assert(device != VK_NULL_HANDLE);
    assert(surface != VK_NULL_HANDLE);

    QueueFamilyIndicies indices;
    auto queue_families = device.getQueueFamilyProperties();
    auto i = 0;
    for (const auto& queue_family : queue_families) {
      if (indices.IsComplete()) {
        break;
      }

      if (queue_family.queueFlags & vk::QueueFlagBits::eGraphics) {
        indices.graphics_family = i;
      }
      if (device.getSurfaceSupportKHR(i, surface)) {
        indices.present_family = i;
      }
      i += 1;
    }
    return indices;
  };

  static auto CheckDeviceExtensionSupport(vk::PhysicalDevice device) -> bool {
    assert(device != VK_NULL_HANDLE);

    auto extension_properties = device.enumerateDeviceExtensionProperties();

    for (const auto& required : DeviceExtensions) {
      auto found = false;
      for (const auto& available : extension_properties) {
        if (required == std::string_view(available.extensionName)) {
          found = true;
        }
      }
      if (not found) {
        return false;
      }
    }
    return true;
  }

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
  GLFWwindow* window_;

  vk::Instance instance_;
  vk::DebugUtilsMessengerEXT debug_messenger_;
  vk::detail::DispatchLoaderDynamic dispatch_loader_;
  vk::PhysicalDevice physical_device_;
  vk::Device device_;
  vk::Queue graphics_queue_;
  vk::Queue present_queue_;

  vk::SurfaceKHR surface_;
  vk::SwapchainKHR swap_chain_;
  std::vector<vk::Image> swap_chain_images_;
  vk::Format swap_chain_image_format_;
  vk::Extent2D swap_chain_extent_;
  std::vector<vk::ImageView> swap_chain_image_views_;
};

}  // namespace nm
