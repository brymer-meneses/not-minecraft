module;

#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_enums.hpp"

export module nm.vk;

namespace nm {

export auto CreateDebugUtilsMessengerEXT(
    vk::Instance instance,
    const vk::DebugUtilsMessengerCreateInfoEXT create_info,
    vk::DebugUtilsMessengerEXT& debug_messenger,
    const vk::AllocationCallbacks allocator = nullptr) -> vk::Result {
  auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));

  if (func != nullptr) {
    auto result =
        func(instance, create_info, allocator,
             reinterpret_cast<VkDebugUtilsMessengerEXT*>(&debug_messenger));
    return static_cast<vk::Result>(result);
  }

  return vk::Result::eErrorExtensionNotPresent;
}

export auto DestroyDebugUtilsMessengerEXT(
    vk::Instance instance, vk::DebugUtilsMessengerEXT debug_messenger,
    const vk::AllocationCallbacks* allocator = nullptr) -> void {
  auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));

  if (func != nullptr) {
    func(instance, debug_messenger, *allocator);
  }
}

}  // namespace nm
