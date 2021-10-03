#include "app.h"

#include <iostream>
#include <vector>

namespace {

const char* kRequiredValidationLayers[] = {
  "VK_LAYER_KHRONOS_validation"
};

bool SupportsValidationLayers() {
  uint32_t layer_count;
  vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

  std::vector<VkLayerProperties> layers(layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

  for (const char* layer_name : kRequiredValidationLayers) {
    bool found = false;
    for (const auto& layer : layers) {
      if (strcmp(layer_name, layer.layerName) == 0) {
        found = true;
        break;
      }
    }
    if (!found) 
      return false;
  }

  return true;
}

std::vector<const char*> GetRequiredValidationLayers() {
  return std::vector<const char*>(
      kRequiredValidationLayers,
      kRequiredValidationLayers + 
          sizeof(kRequiredValidationLayers) / sizeof(const char*));
}

std::vector<const char*> GetRequiredInstanceExtensions() {
  uint32_t glfw_ext_count = 0;
  const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

  std::vector<const char*> extensions(glfw_exts, glfw_exts + glfw_ext_count);
  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  return extensions;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, 
    VkDebugUtilsMessageTypeFlagsEXT type, 
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, 
    void* user_data) {
  std::cerr << "Validation layer: " << callback_data->pMessage << std::endl;
  return VK_FALSE;
}

}  // namespace

bool App::Init() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  window_ = glfwCreateWindow(800, 600, "Hello Triangle", nullptr, nullptr);

  if (!SupportsValidationLayers()) {
    std::cerr << "Does not support required validation layers." << std::endl;
    return false;
  }

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Hello Triangle";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "No Engine";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_0;

  std::vector<const char*> validation_layers =
      GetRequiredValidationLayers();
  std::vector<const char*> instance_exts = 
      GetRequiredInstanceExtensions();

  VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {};
  debug_create_info = {};
  debug_create_info.sType = 
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debug_create_info.messageSeverity = 
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debug_create_info.messageType = 
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debug_create_info.pfnUserCallback = DebugCallback;

  VkInstanceCreateInfo instance_info = {};
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pApplicationInfo = &app_info;
  instance_info.enabledExtensionCount = 
      static_cast<uint32_t>(instance_exts.size());
  instance_info.ppEnabledExtensionNames = instance_exts.data();
  instance_info.enabledLayerCount = 
      static_cast<uint32_t>(validation_layers.size());
  instance_info.ppEnabledLayerNames = validation_layers.data();
  instance_info.pNext = &debug_create_info;

  if (vkCreateInstance(&instance_info, nullptr, &instance_) != VK_SUCCESS) {
    std::cerr << "Failed to create instance." << std::endl;
    return false;
  }

  return true;
}

void App::Destroy() {
  glfwDestroyWindow(window_);

  glfwTerminate();
}

void App::MainLoop() {
  while (!glfwWindowShouldClose(window_)) {
    glfwPollEvents();
  }
}