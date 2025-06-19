module;

#include <iostream>
#include <print>

#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_enums.hpp"

export module nm.engine.vk;

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

export VKAPI_ATTR VkBool32 VKAPI_CALL
DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
              vk::DebugUtilsMessageTypeFlagsEXT message_type,
              const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
              void* user_data) {
  std::println(std::cerr, "Validation Layer: {}", callback_data->pMessage);
  return VK_FALSE;
}

export auto CreateDebugMessengerCreateInfo()
    -> vk::DebugUtilsMessengerCreateInfoEXT {
  vk::DebugUtilsMessengerCreateInfoEXT create_info;

  create_info.messageSeverity =
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;

  create_info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

  create_info.pfnUserCallback = DebugCallback;
  create_info.pUserData = nullptr;

  return create_info;
}

}  // namespace nm
