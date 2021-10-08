#include "point_light/app.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "utils/camera.h"
#include "utils/model.h"

namespace {

const char* kRequiredValidationLayers[] = {
  "VK_LAYER_KHRONOS_validation"
};

const char* kRequiredDeviceExtensions[] = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

constexpr int kShadowTextureWidth = 1024;
constexpr int kShadowTextureHeight = 1024;

constexpr float kShadowPassNearPlane = 0.01f;
constexpr float kShadowPassFarPlane = 10.f;

constexpr int kMaxFramesInFlight = 3;

constexpr float kPi = glm::pi<float>();

bool SupportsValidationLayers() {
  uint32_t count;
  vkEnumerateInstanceLayerProperties(&count, nullptr);

  std::vector<VkLayerProperties> available_layers(count);
  vkEnumerateInstanceLayerProperties(&count, available_layers.data());

  for (const char* required_layer_name : kRequiredValidationLayers) {
    bool found = false;
    for (const auto& available_layer : available_layers) {
      if (strcmp(available_layer.layerName, required_layer_name) == 0) {
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

std::vector<const char*> GetRequiredDeviceExtensions() {
  return std::vector<const char*>(
      kRequiredDeviceExtensions,
      kRequiredDeviceExtensions +
          sizeof(kRequiredDeviceExtensions) / sizeof(const char*));
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
  std::cerr << "Validation layer: " << callback_data->pMessage << std::endl;
  return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* create_info,
    const VkAllocationCallbacks* allocator,
    VkDebugUtilsMessengerEXT* debug_messenger) {
  auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
  if (func != nullptr)
    return func(instance, create_info, allocator, debug_messenger);

  return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debug_messenger,
    const VkAllocationCallbacks* allocator) {
  auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
  if (func != nullptr)
    func(instance, debug_messenger, allocator);
}

struct QueueIndices {
  std::optional<uint32_t> graphics_queue_index;
  std::optional<uint32_t> present_queue_index;
};

bool FoundQueueIndices(const QueueIndices& indices) {
  return indices.graphics_queue_index.has_value() &&
      indices.present_queue_index.has_value();
}

QueueIndices FindQueueIndices(VkPhysicalDevice physical_device,
                              VkSurfaceKHR surface) {
  QueueIndices indices = {};

  uint32_t count;
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);

  std::vector<VkQueueFamilyProperties> families(count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count,
                                           families.data());

  for (int i = 0; i < families.size(); ++i) {
    VkQueueFamilyProperties family = families[i];

    if ((family.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 1)
      indices.graphics_queue_index = i;

    VkBool32 present_support = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface,
                                         &present_support);

    if (present_support)
      indices.present_queue_index = i;

    if (FoundQueueIndices(indices))
      return indices;
  }

  return indices;
}

bool SupportsRequiredDeviceExtensions(VkPhysicalDevice physical_device) {
  uint32_t count;
  vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count,
                                       nullptr);

  std::vector<VkExtensionProperties> available_extensions(count);
  vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count,
                                       available_extensions.data());

  for (const char* required_extension_name :
           GetRequiredDeviceExtensions()) {
    bool found = false;
    for (const auto& available_extension : available_extensions) {
      if (strcmp(available_extension.extensionName,
                 required_extension_name) == 0) {
        found = true;
        break;
      }
    }
    if (!found)
      return false;
  }

  return true;
}

bool IsPhysicalDeviceSuitable(VkPhysicalDevice physical_device,
                              VkSurfaceKHR surface) {
  QueueIndices queue_indices = FindQueueIndices(physical_device, surface);
  if (!FoundQueueIndices(queue_indices))
    return false;

  if (!SupportsRequiredDeviceExtensions(physical_device))
    return false;

  uint32_t surface_formats_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                       &surface_formats_count, nullptr);
  if (surface_formats_count == 0)
    return false;

  uint32_t present_modes_count;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                            &present_modes_count, nullptr);
  if (present_modes_count == 0)
    return false;

  VkPhysicalDeviceFeatures features;
  vkGetPhysicalDeviceFeatures(physical_device, &features);
  if (!features.samplerAnisotropy)
    return false;

  return true;
}

VkSampleCountFlagBits ChooseMsaaSampleCount(VkPhysicalDevice physical_device) {
  VkPhysicalDeviceProperties phys_device_props;
  vkGetPhysicalDeviceProperties(physical_device, &phys_device_props);

  VkSampleCountFlags sample_count_flags =
      phys_device_props.limits.framebufferColorSampleCounts &
      phys_device_props.limits.framebufferDepthSampleCounts;

  if (sample_count_flags & VK_SAMPLE_COUNT_4_BIT) {
    return VK_SAMPLE_COUNT_4_BIT;
  } else if (sample_count_flags & VK_SAMPLE_COUNT_2_BIT) {
    return VK_SAMPLE_COUNT_2_BIT;
  }
  return VK_SAMPLE_COUNT_1_BIT;
}

VkSurfaceFormatKHR ChooseSurfaceFormat(VkPhysicalDevice physical_device,
                                       VkSurfaceKHR surface) {
  uint32_t formats_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &formats_count,
                                       nullptr);

  std::vector<VkSurfaceFormatKHR> formats(formats_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &formats_count,
                                       formats.data());

  for (const auto& format : formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      return format;
  }
  return formats[0];
}

VkPresentModeKHR ChoosePresentMode(VkPhysicalDevice physical_device,
                                   VkSurfaceKHR surface) {
  uint32_t present_mode_count;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                            &present_mode_count, nullptr);

  std::vector<VkPresentModeKHR> present_modes(present_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                            &present_mode_count,
                                            present_modes.data());

  for (const auto& mode : present_modes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
      return mode;
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseSwapChainExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                 GLFWwindow* window) {
  if (capabilities.currentExtent.width != UINT32_MAX) {
    return capabilities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D extent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };
    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height,
                               capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);

    return extent;
  }
}

VkFormat FindSupportedFormat(const std::vector<VkFormat>& formats,
                             VkImageTiling tiling,
                             VkFormatFeatureFlags features,
                             VkPhysicalDevice physical_device) {
  for (const auto& format : formats) {
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);

    if (tiling == VK_IMAGE_TILING_LINEAR &&
        (properties.linearTilingFeatures & features) == features) {
      return format;
    } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
               (properties.optimalTilingFeatures & features) == features) {
      return format;
    }
  }
  return VK_FORMAT_UNDEFINED;
}

VkFormat FindDepthFormat(VkPhysicalDevice physical_device) {
  std::vector<VkFormat> formats = {
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT
  };
  return FindSupportedFormat(formats, VK_IMAGE_TILING_OPTIMAL,
                             VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
                             physical_device);
}

std::vector<char> LoadShaderFile(const std::string& path) {
  std::vector<char> buffer;

  std::ifstream strm(path, std::ios::ate | std::ios::binary);
  if (!strm.is_open())
    return buffer;

  size_t file_size = strm.tellg();
  buffer.resize(file_size);

  strm.seekg(0);
  strm.read(buffer.data(), file_size);
  strm.close();

  return buffer;
}

bool CreateShaderModulesFromFiles(const std::vector<std::string>& file_paths,
                                  VkDevice device,
                                  std::vector<VkShaderModule>* shader_modules) {
  shader_modules->clear();

  for (const std::string& path : file_paths) {
    std::vector<char> data = LoadShaderFile(path);
    if (data.empty())
      return false;

    VkShaderModuleCreateInfo shader_module_info = {};
    shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_info.codeSize = data.size();
    shader_module_info.pCode = reinterpret_cast<const uint32_t*>(data.data());

    VkShaderModule shader_module;
    if (vkCreateShaderModule(device, &shader_module_info, nullptr,
                             &shader_module) != VK_SUCCESS) {
      return false;
    }

    shader_modules->push_back(shader_module);
  }
  return true;
}

bool AllocateMemoryForResource(VkMemoryPropertyFlags mem_properties,
                               VkMemoryRequirements mem_requirements,
                               VkPhysicalDevice physical_device,
                               VkDevice device, VkDeviceMemory* memory) {
  VkPhysicalDeviceMemoryProperties phys_device_mem_props;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &phys_device_mem_props);

  std::optional<uint32_t> memory_type_index;
  for (int i = 0; i < phys_device_mem_props.memoryTypeCount; i++) {
    if ((mem_requirements.memoryTypeBits & (1 << i)) &&
        (phys_device_mem_props.memoryTypes[i].propertyFlags & mem_properties)
             == mem_properties) {
      memory_type_index = i;
      break;
    }
  }
  if (!memory_type_index.has_value())
    return false;

  VkMemoryAllocateInfo mem_alloc_info = {};
  mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_alloc_info.allocationSize = mem_requirements.size;
  mem_alloc_info.memoryTypeIndex = memory_type_index.value();

  if (vkAllocateMemory(device, &mem_alloc_info, nullptr, memory) != VK_SUCCESS)
    return false;

  return true;
}

bool CreateImage(VkImageCreateInfo* image_info,
                 VkMemoryPropertyFlags mem_properties,
                 VkPhysicalDevice physical_device, VkDevice device,
                 VkImage* image, VkDeviceMemory* memory) {
  if (vkCreateImage(device, image_info, nullptr, image) != VK_SUCCESS)
    return false;

  VkMemoryRequirements mem_requirements;
  vkGetImageMemoryRequirements(device, *image, &mem_requirements);

  if (!AllocateMemoryForResource(mem_properties, mem_requirements,
                                 physical_device, device, memory))
    return false;
  vkBindImageMemory(device, *image, *memory, 0);

  return true;
}

bool CreateBuffer(VkBufferCreateInfo* buffer_info,
                  VkMemoryPropertyFlags mem_properties,
                  VkPhysicalDevice physical_device, VkDevice device,
                  VkBuffer* buffer, VkDeviceMemory* memory) {
  if (vkCreateBuffer(device, buffer_info, nullptr, buffer) != VK_SUCCESS)
    return false;

  VkMemoryRequirements mem_requirements;
  vkGetBufferMemoryRequirements(device, *buffer, &mem_requirements);

 if (!AllocateMemoryForResource(mem_properties, mem_requirements,
                                 physical_device, device, memory))
    return false;
  vkBindBufferMemory(device, *buffer, *memory, 0);

  return true;
}

}  // namespace

bool App::Init() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_SCALE_TO_MONITOR , GL_TRUE);

  window_ = glfwCreateWindow(800, 600, "Vulkan Application", nullptr, nullptr);
  glfwSetWindowUserPointer(window_, this);
  glfwSetFramebufferSizeCallback(window_, GlfwFramebufferResized);

if (!utils::LoadModel("cornell_box.obj", &model_))
    return false;

  camera_.SetPosition(glm::vec3(0.f, 1.f, 4.f));

  if (!InitInstanceAndSurface())
    return false;

  if (!ChoosePhysicalDevice())
    return false;

  if (!CreateDevice())
    return false;

  if (!CreateSwapChain())
    return false;

  if (!CreateScenePassResources())
    return false;

  if (!CreateShadowPassResources())
    return false;

  if (!CreateCommandPool())
    return false;

  if (!CreateCommandBuffers())
    return false;

  if (!CreateDescriptorSets())
    return false;

  if (!CreateVertexBuffers())
    return false;

  if (!RecordCommandBuffers())
    return false;

  if (!CreateSyncObjects())
    return false;

  return true;
}

void App::GlfwFramebufferResized(GLFWwindow* window, int width, int height) {
  auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
  app->framebuffer_resized_ = true;
}

bool App::InitInstanceAndSurface() {
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
  std::vector<const char*> instance_extensions =
      GetRequiredInstanceExtensions();

  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {};
  debug_messenger_info = {};
  debug_messenger_info.sType =
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debug_messenger_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debug_messenger_info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debug_messenger_info.pfnUserCallback = DebugCallback;

  VkInstanceCreateInfo instance_info = {};
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pApplicationInfo = &app_info;
  instance_info.enabledExtensionCount =
      static_cast<uint32_t>(instance_extensions.size());
  instance_info.ppEnabledExtensionNames = instance_extensions.data();
  instance_info.enabledLayerCount =
      static_cast<uint32_t>(validation_layers.size());
  instance_info.ppEnabledLayerNames = validation_layers.data();
  instance_info.pNext = &debug_messenger_info;

  if (vkCreateInstance(&instance_info, nullptr, &instance_) != VK_SUCCESS) {
    std::cerr << "Could not create instance." << std::endl;
    return false;
  }

  if (CreateDebugUtilsMessengerEXT(instance_, &debug_messenger_info, nullptr,
                                   &debug_messenger_) != VK_SUCCESS) {
    std::cerr << "Could not create debug messenger." << std::endl;
    return false;
  }

  if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_)
          != VK_SUCCESS) {
    std::cerr << "Could not create surface." << std::endl;
    return false;
  }
  return true;
}

bool App::ChoosePhysicalDevice() {
  uint32_t phys_devices_count = 0;
  vkEnumeratePhysicalDevices(instance_, &phys_devices_count, nullptr);
  if (phys_devices_count == 0) {
    std::cerr << "Could not find suitable physical device." << std::endl;
    return false;
  }

  std::vector<VkPhysicalDevice> phys_devices(phys_devices_count);
  vkEnumeratePhysicalDevices(instance_, &phys_devices_count,
                             phys_devices.data());

  for (const auto& phys_device : phys_devices) {
    if (IsPhysicalDeviceSuitable(phys_device, surface_)) {
      physical_device_ = phys_device;
      return true;
    }
  }
  std::cerr << "Could not find suitable physical device." << std::endl;
  return false;
}

bool App::CreateDevice() {
  QueueIndices queue_indices = FindQueueIndices(physical_device_, surface_);
  graphics_queue_index_ = queue_indices.graphics_queue_index.value();
  present_queue_index_ = queue_indices.present_queue_index.value();

  std::set<uint32_t> unique_queue_indices = {
    graphics_queue_index_, present_queue_index_
  };

  std::vector<VkDeviceQueueCreateInfo> queue_infos;
  for (uint32_t queue_index : unique_queue_indices) {
    float priority = 1.0f;

    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = queue_index;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;
    queue_infos.push_back(queue_info);
  }

  VkPhysicalDeviceFeatures phys_device_features = {};
  phys_device_features.samplerAnisotropy = VK_TRUE;

  std::vector<const char*> device_extensions = GetRequiredDeviceExtensions();
  std::vector<const char*> validation_layers = GetRequiredValidationLayers();

  VkDeviceCreateInfo device_info = {};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
  device_info.pQueueCreateInfos = queue_infos.data();
  device_info.pEnabledFeatures = &phys_device_features;
  device_info.enabledExtensionCount = static_cast<uint32_t>(
      device_extensions.size());
  device_info.ppEnabledExtensionNames = device_extensions.data();
  device_info.enabledLayerCount = static_cast<uint32_t>(
      validation_layers.size());
  device_info.ppEnabledLayerNames = validation_layers.data();

  if (vkCreateDevice(physical_device_, &device_info, nullptr, &device_)
          != VK_SUCCESS) {
    std::cerr << "Could not create device." << std::endl;
    return false;
  }

  vkGetDeviceQueue(device_, graphics_queue_index_, 0, &graphics_queue_);
  vkGetDeviceQueue(device_, present_queue_index_, 0, &present_queue_);

  msaa_sample_count_ = ChooseMsaaSampleCount(physical_device_);

  return true;
}

bool App::CreateSwapChain() {
  VkSurfaceCapabilitiesKHR surface_capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_,
                                            &surface_capabilities);

  swap_chain_extent_ = ChooseSwapChainExtent(surface_capabilities, window_);
  uint32_t swap_chain_image_count = std::min(
      surface_capabilities.minImageCount + 1,
      surface_capabilities.maxImageCount);

  VkSurfaceFormatKHR surface_format = ChooseSurfaceFormat(physical_device_,
                                                          surface_);
  swap_chain_image_format_ = surface_format.format;

  VkPresentModeKHR present_mode = ChoosePresentMode(physical_device_, surface_);

  VkSwapchainCreateInfoKHR swap_chain_info = {};
  swap_chain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swap_chain_info.surface = surface_;
  swap_chain_info.minImageCount = swap_chain_image_count;
  swap_chain_info.imageFormat = swap_chain_image_format_;
  swap_chain_info.imageColorSpace = surface_format.colorSpace;
  swap_chain_info.imageExtent = swap_chain_extent_;
  swap_chain_info.imageArrayLayers = 1;
  swap_chain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swap_chain_info.preTransform = surface_capabilities.currentTransform;
  swap_chain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swap_chain_info.presentMode = present_mode;
  swap_chain_info.clipped = VK_TRUE;

  if (graphics_queue_index_ != present_queue_index_) {
    uint32_t indices[] = { graphics_queue_index_, present_queue_index_ };
    swap_chain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swap_chain_info.queueFamilyIndexCount = 2;
    swap_chain_info.pQueueFamilyIndices = indices;
  } else {
    swap_chain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  if (vkCreateSwapchainKHR(device_, &swap_chain_info, nullptr, &swap_chain_)
          != VK_SUCCESS) {
    std::cerr << "Could not create swap chain." << std::endl;
    return false;
  }

  vkGetSwapchainImagesKHR(device_, swap_chain_, &swap_chain_image_count,
                          nullptr);
  swap_chain_images_.resize(swap_chain_image_count);
  vkGetSwapchainImagesKHR(device_, swap_chain_, &swap_chain_image_count,
                          swap_chain_images_.data());

  swap_chain_image_views_.resize(swap_chain_images_.size());

  for (int i = 0; i < swap_chain_images_.size(); ++i) {
    VkImageViewCreateInfo image_view_info = {};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = swap_chain_images_[i];
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = swap_chain_image_format_;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device_, &image_view_info, nullptr,
                         &swap_chain_image_views_[i]) != VK_SUCCESS) {
      std::cerr << "Could not create swap chain image view." << std::endl;
      return false;
    }
  }
  return true;
}

bool App::CreateScenePassResources() {
  if (!CreateRenderPass())
    return false;

  if (!CreatePipeline())
    return false;

  if (!CreateFramebuffers())
    return false;

  return true;
}

bool App::CreateRenderPass() {
  VkAttachmentDescription color_attachment = {};
  color_attachment.format = swap_chain_image_format_;
  color_attachment.samples = msaa_sample_count_;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkFormat depth_format = FindDepthFormat(physical_device_);
  if (depth_format == VK_FORMAT_UNDEFINED) {
    std::cerr << "Could not find suitable depth format." << std::endl;
    return false;
  }

  VkAttachmentDescription depth_attachment = {};
  depth_attachment.format = depth_format;
  depth_attachment.samples = msaa_sample_count_;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription color_resolve_attachment = {};
  color_resolve_attachment.format = swap_chain_image_format_;
  color_resolve_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_resolve_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_resolve_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_resolve_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_resolve_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_ref = {};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_attachment_ref = {};
  depth_attachment_ref.attachment = 1;
  depth_attachment_ref.layout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference color_resolve_attachment_ref = {};
  color_resolve_attachment_ref.attachment = 2;
  color_resolve_attachment_ref.layout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;
  subpass.pResolveAttachments = &color_resolve_attachment_ref;
  subpass.pDepthStencilAttachment = &depth_attachment_ref;

  VkSubpassDependency subpass_dep = {};
  subpass_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  subpass_dep.dstSubpass = 0;
  subpass_dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  subpass_dep.srcAccessMask = 0;
  subpass_dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  subpass_dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  VkAttachmentDescription attachments[] = {
    color_attachment, depth_attachment, color_resolve_attachment
  };

  VkRenderPassCreateInfo render_pass_info = {};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 3;
  render_pass_info.pAttachments = attachments;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &subpass_dep;

  if (vkCreateRenderPass(device_, &render_pass_info, nullptr, &render_pass_)
          != VK_SUCCESS) {
    std::cerr << "Could not create render pass." << std::endl;
    return false;
  }
  return true;
}

bool App::CreatePipeline() {
  std::vector<std::string> shader_file_paths = {
    "shader_vert.spv", "shader_frag.spv"
  };
  std::vector<VkShaderModule> shader_modules;
  if (!CreateShaderModulesFromFiles(shader_file_paths, device_,
                                    &shader_modules)) {
    std::cerr << "Could not create shader modules." << std::endl;
    return false;
  }

  VkPipelineShaderStageCreateInfo vert_shader_info = {};
  vert_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vert_shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vert_shader_info.module = shader_modules[0];
  vert_shader_info.pName = "main";

  VkPipelineShaderStageCreateInfo frag_shader_info = {};
  frag_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  frag_shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  frag_shader_info.module = shader_modules[1];
  frag_shader_info.pName = "main";

  VkPipelineShaderStageCreateInfo shader_stages[] = {
    vert_shader_info, frag_shader_info
  };

  VkDescriptorSetLayoutBinding vert_ubo_binding = {};
  vert_ubo_binding.binding = 0;
  vert_ubo_binding.descriptorCount = 1;
  vert_ubo_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  vert_ubo_binding.pImmutableSamplers = nullptr;
  vert_ubo_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutBinding frag_ubo_binding = {};
  frag_ubo_binding.binding = 1;
  frag_ubo_binding.descriptorCount = 1;
  frag_ubo_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  frag_ubo_binding.pImmutableSamplers = nullptr;
  frag_ubo_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutBinding shadow_tex_sampler_binding = {};
  shadow_tex_sampler_binding.binding = 2;
  shadow_tex_sampler_binding.descriptorCount = 1;
  shadow_tex_sampler_binding.descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  shadow_tex_sampler_binding.pImmutableSamplers = nullptr;
  shadow_tex_sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutBinding descriptor_set_bindings[] = {
    vert_ubo_binding, frag_ubo_binding, shadow_tex_sampler_binding
  };

  VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {};
  descriptor_set_layout_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptor_set_layout_info.bindingCount = 3;
  descriptor_set_layout_info.pBindings = descriptor_set_bindings;

  if (vkCreateDescriptorSetLayout(device_, &descriptor_set_layout_info, nullptr,
                                  &descriptor_set_layout_) != VK_SUCCESS) {
    std::cerr << "Could not create descriptor set layout." << std::endl;
    return false;
  }

  VkVertexInputBindingDescription position_binding = {};
  position_binding.binding = 0;
  position_binding.stride = sizeof(glm::vec3);
  position_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription position_attrib_desc = {};
  position_attrib_desc.binding = 0;
  position_attrib_desc.location = 0;
  position_attrib_desc.format = VK_FORMAT_R32G32B32_SFLOAT;
  position_attrib_desc.offset = 0;

  VkVertexInputBindingDescription normal_binding = {};
  normal_binding.binding = 1;
  normal_binding.stride = sizeof(glm::vec3);
  normal_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription normal_attrib_desc = {};
  normal_attrib_desc.binding = 1;
  normal_attrib_desc.location = 1;
  normal_attrib_desc.format = VK_FORMAT_R32G32B32_SFLOAT;
  normal_attrib_desc.offset = 0;

  VkVertexInputBindingDescription mtl_idx_binding = {};
  mtl_idx_binding.binding = 2;
  mtl_idx_binding.stride = sizeof(uint32_t);
  mtl_idx_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription mtl_idx_attrib_desc = {};
  mtl_idx_attrib_desc.binding = 2;
  mtl_idx_attrib_desc.location = 2;
  mtl_idx_attrib_desc.format = VK_FORMAT_R32_UINT;
  mtl_idx_attrib_desc.offset = 0;

  VkVertexInputBindingDescription vertex_bindings[] = {
    position_binding, normal_binding, mtl_idx_binding
  };

  VkVertexInputAttributeDescription vertex_attribs[] = {
    position_attrib_desc, normal_attrib_desc, mtl_idx_attrib_desc
  };

  VkPipelineVertexInputStateCreateInfo vertex_input = {};
  vertex_input.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input.vertexBindingDescriptionCount = 3;
  vertex_input.pVertexBindingDescriptions = vertex_bindings;
  vertex_input.vertexAttributeDescriptionCount = 3;
  vertex_input.pVertexAttributeDescriptions = vertex_attribs;

  VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport = {};
  viewport.x = 0.f;
  viewport.y = 0.f;
  viewport.width = static_cast<float>(swap_chain_extent_.width);
  viewport.height = static_cast<float>(swap_chain_extent_.height);
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;

  VkRect2D scissor = {};
  scissor.offset = {0, 0};
  scissor.extent = swap_chain_extent_;

  VkPipelineViewportStateCreateInfo viewport_info = {};
  viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_info.viewportCount = 1;
  viewport_info.pViewports = &viewport;
  viewport_info.scissorCount = 1;
  viewport_info.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling = {};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = msaa_sample_count_;

  VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
  depth_stencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.depthTestEnable = VK_TRUE;
  depth_stencil.depthWriteEnable = VK_TRUE;
  depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable = VK_FALSE;

  VkPipelineColorBlendAttachmentState color_blend_attachment = {};
  color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo color_blend_info = {};
  color_blend_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend_info.logicOpEnable = VK_FALSE;
  color_blend_info.logicOp = VK_LOGIC_OP_COPY;
  color_blend_info.attachmentCount = 1;
  color_blend_info.pAttachments = &color_blend_attachment;
  color_blend_info.blendConstants[0] = 0.0f;
  color_blend_info.blendConstants[1] = 0.0f;
  color_blend_info.blendConstants[2] = 0.0f;
  color_blend_info.blendConstants[3] = 0.0f;

  VkPipelineLayoutCreateInfo pipeline_layout_info = {};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = &descriptor_set_layout_;

  if (vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr,
                             &pipeline_layout_) != VK_SUCCESS) {
    std::cerr << "Could not create pipeline layout." << std::endl;
    return false;
  }

  VkGraphicsPipelineCreateInfo pipeline_info = {};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = 2;
  pipeline_info.pStages = shader_stages;
  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_info;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pDepthStencilState = &depth_stencil;
  pipeline_info.pColorBlendState = &color_blend_info;
  pipeline_info.layout = pipeline_layout_;
  pipeline_info.renderPass = render_pass_;
  pipeline_info.subpass = 0;
  pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info,
                                nullptr, &pipeline_) != VK_SUCCESS) {
    std::cerr << "Could not create pipeline." << std::endl;
    return false;
  }

  for (VkShaderModule shader_module : shader_modules) {
    vkDestroyShaderModule(device_, shader_module, nullptr);
  }
  return true;
}

bool App::CreateFramebuffers() {
  VkImageCreateInfo color_image_info = {};
  color_image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  color_image_info.imageType = VK_IMAGE_TYPE_2D;
  color_image_info.extent.width = swap_chain_extent_.width;
  color_image_info.extent.height = swap_chain_extent_.height;
  color_image_info.extent.depth = 1;
  color_image_info.mipLevels = 1;
  color_image_info.arrayLayers = 1;
  color_image_info.format = swap_chain_image_format_;
  color_image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  color_image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_image_info.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  color_image_info.samples = msaa_sample_count_;
  color_image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (!CreateImage(&color_image_info,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, physical_device_,
                   device_, &color_image_,
                   &color_image_memory_)) {
    std::cerr << "Could not create color image." << std::endl;
    return false;
  }

  VkImageViewCreateInfo color_image_view_info = {};
  color_image_view_info.sType =
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  color_image_view_info.image = color_image_;
  color_image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  color_image_view_info.format = swap_chain_image_format_;
  color_image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  color_image_view_info.subresourceRange.baseMipLevel = 0;
  color_image_view_info.subresourceRange.levelCount = 1;
  color_image_view_info.subresourceRange.baseArrayLayer = 0;
  color_image_view_info.subresourceRange.layerCount = 1;

  if (vkCreateImageView(device_, &color_image_view_info, nullptr,
                        &color_image_view_) != VK_SUCCESS) {
    std::cerr << "Could not create color image view." << std::endl;
    return false;
  }

  VkFormat depth_format = FindDepthFormat(physical_device_);
  if (depth_format == VK_FORMAT_UNDEFINED) {
    std::cerr << "Could not find suitable depth format." << std::endl;
    return false;
  }

  VkImageCreateInfo depth_image_info = {};
  depth_image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  depth_image_info.imageType = VK_IMAGE_TYPE_2D;
  depth_image_info.extent.width = swap_chain_extent_.width;
  depth_image_info.extent.height = swap_chain_extent_.height;
  depth_image_info.extent.depth = 1;
  depth_image_info.mipLevels = 1;
  depth_image_info.arrayLayers = 1;
  depth_image_info.format = depth_format;
  depth_image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  depth_image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  depth_image_info.samples = msaa_sample_count_;
  depth_image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (!CreateImage(&depth_image_info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                   physical_device_, device_, &depth_image_,
                   &depth_image_memory_)) {
    std::cerr << "Could not create depth image." << std::endl;
    return false;
  }

  VkImageViewCreateInfo depth_image_view_info = {};
  depth_image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  depth_image_view_info.image = depth_image_;
  depth_image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  depth_image_view_info.format = depth_format;
  depth_image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  depth_image_view_info.subresourceRange.baseMipLevel = 0;
  depth_image_view_info.subresourceRange.levelCount = 1;
  depth_image_view_info.subresourceRange.baseArrayLayer = 0;
  depth_image_view_info.subresourceRange.layerCount = 1;

  if (vkCreateImageView(device_, &depth_image_view_info, nullptr,
                        &depth_image_view_) != VK_SUCCESS) {
    std::cerr << "Could not create depth image view." << std::endl;
    return false;
  }

  swap_chain_framebuffers_.resize(swap_chain_images_.size());

  for (int i = 0; i < swap_chain_images_.size(); ++i) {
    VkImageView attachments[] = {
      color_image_view_, depth_image_view_, swap_chain_image_views_[i]
    };

    VkFramebufferCreateInfo framebuffer_info = {};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = render_pass_;
    framebuffer_info.attachmentCount = 3;
    framebuffer_info.pAttachments = attachments;
    framebuffer_info.width = swap_chain_extent_.width;
    framebuffer_info.height = swap_chain_extent_.height;
    framebuffer_info.layers = 1;

    if (vkCreateFramebuffer(device_, &framebuffer_info, nullptr,
                            &swap_chain_framebuffers_[i]) != VK_SUCCESS) {
      std::cerr << "Could not create framebuffer." << std::endl;
      return false;
    }
  }
  return true;
}

bool App::CreateShadowPassResources() {
  if (!CreateShadowRenderPass())
    return false;

  if (!CreateShadowPipeline())
    return false;

  if (!CreateShadowFramebuffers())
    return false;

  return true;
}

bool App::CreateShadowRenderPass() {
  VkFormat depth_format = FindDepthFormat(physical_device_);
  if (depth_format == VK_FORMAT_UNDEFINED) {
    std::cerr << "Could not find suitable depth format." << std::endl;
    return false;
  }

  VkAttachmentDescription depth_attachment = {};
  depth_attachment.format = depth_format;
  depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_attachment_ref = {};
  depth_attachment_ref.attachment = 0;
  depth_attachment_ref.layout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.pDepthStencilAttachment = &depth_attachment_ref;

  VkSubpassDependency subpass_dep = {};
  subpass_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  subpass_dep.dstSubpass = 0;
  subpass_dep.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  subpass_dep.srcAccessMask = 0;
  subpass_dep.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  subpass_dep.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo render_pass_info = {};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &depth_attachment;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &subpass_dep;

  if (vkCreateRenderPass(device_, &render_pass_info, nullptr,
                         &shadow_render_pass_) != VK_SUCCESS) {
    std::cerr << "Could not create shadow render pass." << std::endl;
    return false;
  }
  return true;
}

bool App::CreateShadowPipeline() {
  std::vector<std::string> shader_file_paths = {
    "shadow_vert.spv", "shadow_frag.spv"
  };
  std::vector<VkShaderModule> shader_modules;
  if (!CreateShaderModulesFromFiles(shader_file_paths, device_,
                                    &shader_modules)) {
    std::cerr << "Could not create shadow shader modules." << std::endl;
    return false;
  }

  VkPipelineShaderStageCreateInfo vert_shader_info = {};
  vert_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vert_shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vert_shader_info.module = shader_modules[0];
  vert_shader_info.pName = "main";

  VkPipelineShaderStageCreateInfo frag_shader_info = {};
  frag_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  frag_shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  frag_shader_info.module = shader_modules[1];
  frag_shader_info.pName = "main";

  VkPipelineShaderStageCreateInfo shader_stages[] = {
    vert_shader_info, frag_shader_info
  };

  VkDescriptorSetLayoutBinding vert_ubo_binding = {};
  vert_ubo_binding.binding = 0;
  vert_ubo_binding.descriptorCount = 1;
  vert_ubo_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  vert_ubo_binding.pImmutableSamplers = nullptr;
  vert_ubo_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutCreateInfo descriptor_layout_info = {};
  descriptor_layout_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptor_layout_info.bindingCount = 1;
  descriptor_layout_info.pBindings = &vert_ubo_binding;

  if (vkCreateDescriptorSetLayout(device_, &descriptor_layout_info, nullptr,
                                  &shadow_descriptor_layout_) != VK_SUCCESS) {
    std::cerr << "Could not create shadow descriptor set layout." << std::endl;
    return false;
  }

  VkVertexInputBindingDescription vertex_pos_binding = {};
  vertex_pos_binding.binding = 0;
  vertex_pos_binding.stride = sizeof(glm::vec3);
  vertex_pos_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription vertex_pos_desc = {};
  vertex_pos_desc.binding = 0;
  vertex_pos_desc.location = 0;
  vertex_pos_desc.format = VK_FORMAT_R32G32B32_SFLOAT;
  vertex_pos_desc.offset = 0;

  VkPipelineVertexInputStateCreateInfo vertex_input = {};
  vertex_input.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input.vertexBindingDescriptionCount = 1;
  vertex_input.pVertexBindingDescriptions = &vertex_pos_binding;
  vertex_input.vertexAttributeDescriptionCount = 1;
  vertex_input.pVertexAttributeDescriptions = &vertex_pos_desc;

  VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport = {};
  viewport.x = 0.f;
  viewport.y = 0.f;
  viewport.width = static_cast<float>(kShadowTextureWidth);
  viewport.height = static_cast<float>(kShadowTextureHeight);
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;

  VkRect2D scissor = {};
  scissor.offset = {0, 0};
  scissor.extent = {kShadowTextureWidth, kShadowTextureHeight};

  VkPipelineViewportStateCreateInfo viewport_info = {};
  viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_info.viewportCount = 1;
  viewport_info.pViewports = &viewport;
  viewport_info.scissorCount = 1;
  viewport_info.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling = {};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
  depth_stencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.depthTestEnable = VK_TRUE;
  depth_stencil.depthWriteEnable = VK_TRUE;
  depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable = VK_FALSE;

  VkPushConstantRange push_constant_range = {};
  push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  push_constant_range.offset = 0;
  push_constant_range.size = sizeof(glm::mat4);

  VkPipelineLayoutCreateInfo pipeline_layout_info = {};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.pushConstantRangeCount = 1;
  pipeline_layout_info.pPushConstantRanges = &push_constant_range;

  if (vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr,
                             &shadow_pipeline_layout_) != VK_SUCCESS) {
    std::cerr << "Could not create shadow pipeline layout." << std::endl;
    return false;
  }

  VkGraphicsPipelineCreateInfo pipeline_info = {};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = 2;
  pipeline_info.pStages = shader_stages;
  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_info;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pDepthStencilState = &depth_stencil;
  pipeline_info.layout = shadow_pipeline_layout_;
  pipeline_info.renderPass = shadow_render_pass_;
  pipeline_info.subpass = 0;
  pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info,
                                nullptr, &shadow_pipeline_) != VK_SUCCESS) {
    std::cerr << "Could not create shadow pipeline." << std::endl;
    return false;
  }

  for (VkShaderModule shader_module : shader_modules) {
    vkDestroyShaderModule(device_, shader_module, nullptr);
  }
  return true;
}

bool App::CreateShadowFramebuffers() {
  VkFormat depth_format = FindDepthFormat(physical_device_);
  if (depth_format == VK_FORMAT_UNDEFINED) {
    std::cerr << "Could not find suitable depth format." << std::endl;
    return false;
  }

  shadow_frame_resources_.resize(swap_chain_images_.size());

  for (ShadowPassFrameResource& frame : shadow_frame_resources_) {
    VkImageCreateInfo shadow_tex_info = {};
    shadow_tex_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    shadow_tex_info.imageType = VK_IMAGE_TYPE_2D;
    shadow_tex_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    shadow_tex_info.extent.width = kShadowTextureWidth;
    shadow_tex_info.extent.height = kShadowTextureHeight;
    shadow_tex_info.extent.depth = 1;
    shadow_tex_info.mipLevels = 1;
    shadow_tex_info.arrayLayers = 6;
    shadow_tex_info.format = depth_format;
    shadow_tex_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    shadow_tex_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    shadow_tex_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT;
    shadow_tex_info.samples = VK_SAMPLE_COUNT_1_BIT;
    shadow_tex_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (!CreateImage(&shadow_tex_info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    physical_device_, device_, &frame.shadow_texture,
                    &frame.shadow_texture_memory)) {
      std::cerr << "Could not create shadow image." << std::endl;
      return false;
    }

    frame.depth_framebuffer_views.resize(6);
    frame.depth_framebuffers.resize(6);

    for (int i = 0; i < frame.depth_framebuffer_views.size(); ++i) {
      VkImageViewCreateInfo image_view_info = {};
      image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      image_view_info.image = frame.shadow_texture;
      image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      image_view_info.format = depth_format;
      image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      image_view_info.subresourceRange.baseMipLevel = 0;
      image_view_info.subresourceRange.levelCount = 1;
      image_view_info.subresourceRange.baseArrayLayer = i;
      image_view_info.subresourceRange.layerCount = 1;

      if (vkCreateImageView(device_, &image_view_info, nullptr,
                            &frame.depth_framebuffer_views[i]) != VK_SUCCESS) {
        std::cerr << "Could not create shadow image framebuffer view."
                  << std::endl;
        return false;
      }

      VkFramebufferCreateInfo framebuffer_info = {};
      framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebuffer_info.renderPass = shadow_render_pass_;
      framebuffer_info.attachmentCount = 1;
      framebuffer_info.pAttachments = &frame.depth_framebuffer_views[i];
      framebuffer_info.width = kShadowTextureWidth;
      framebuffer_info.height = kShadowTextureHeight;
      framebuffer_info.layers = 1;

      if (vkCreateFramebuffer(device_, &framebuffer_info, nullptr,
                              &frame.depth_framebuffers[i]) != VK_SUCCESS) {
        std::cerr << "Could not create framebuffer." << std::endl;
        return false;
      }
    }

    VkImageViewCreateInfo shadow_tex_view_info = {};
    shadow_tex_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    shadow_tex_view_info.image = frame.shadow_texture;
    shadow_tex_view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    shadow_tex_view_info.format = depth_format;
    shadow_tex_view_info.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_DEPTH_BIT;
    shadow_tex_view_info.subresourceRange.baseMipLevel = 0;
    shadow_tex_view_info.subresourceRange.levelCount = 1;
    shadow_tex_view_info.subresourceRange.baseArrayLayer = 0;
    shadow_tex_view_info.subresourceRange.layerCount = 6;

    if (vkCreateImageView(device_, &shadow_tex_view_info, nullptr,
                          &frame.shadow_texture_view) != VK_SUCCESS) {
      std::cerr << "Could not create shadow texture view." << std::endl;
      return false;
    }
  }

  return true;
}

bool App::CreateCommandPool() {
  VkCommandPoolCreateInfo command_pool_info = {};
  command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_info.queueFamilyIndex = graphics_queue_index_;

  if (vkCreateCommandPool(device_, &command_pool_info, nullptr, &command_pool_)
          != VK_SUCCESS) {
    std::cerr << "Could not create command pool." << std::endl;
    return false;
  }
  return true;
}

bool App::CreateCommandBuffers() {
  command_buffers_.resize(swap_chain_framebuffers_.size());

  VkCommandBufferAllocateInfo command_buffer_info = {};
  command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_info.commandPool = command_pool_;
  command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  command_buffer_info.commandBufferCount =
      static_cast<uint32_t>(command_buffers_.size());

  if (vkAllocateCommandBuffers(device_, &command_buffer_info,
                               command_buffers_.data()) != VK_SUCCESS) {
    std::cerr << "Could not create command buffers." << std::endl;
    return false;
  }
  return true;
}

bool App::CreateDescriptorSets() {
  VkDescriptorPoolSize uniform_buffer_pool_size = {};
  uniform_buffer_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uniform_buffer_pool_size.descriptorCount =
      static_cast<uint32_t>(swap_chain_images_.size()) * 2;

  VkDescriptorPoolSize combined_sampler_pool_size = {};
  combined_sampler_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  combined_sampler_pool_size.descriptorCount =
      static_cast<uint32_t>(swap_chain_images_.size());

  VkDescriptorPoolSize pool_sizes[] = {
    uniform_buffer_pool_size, combined_sampler_pool_size
  };

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 2;
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = static_cast<uint32_t>(swap_chain_images_.size());

  if (vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_)
          != VK_SUCCESS) {
    std::cerr << "Could not create descriptor pool." << std::endl;
    return false;
  }

  std::vector<VkDescriptorSetLayout> layouts(swap_chain_images_.size(),
                                             descriptor_set_layout_);
  VkDescriptorSetAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = descriptor_pool_;
  alloc_info.descriptorSetCount = static_cast<uint32_t>(
      swap_chain_images_.size());
  alloc_info.pSetLayouts = layouts.data();

  descriptor_sets_.resize(swap_chain_images_.size());
  if (vkAllocateDescriptorSets(device_, &alloc_info, descriptor_sets_.data())
          != VK_SUCCESS) {
    std::cerr << "Could not create descriptor sets." << std::endl;
    return false;
  }

  struct VertexShaderUbo {
    glm::mat4 model_mat;
    glm::mat4 mvp_mat;
  } vert_ubo_data;

  float aspect_ratio = static_cast<float>(swap_chain_extent_.width) /
      static_cast<float>(swap_chain_extent_.height);

  glm::mat4 model_mat = glm::mat4(1.f);
  glm::mat4 view_mat = camera_.GetViewMat();
  glm::mat4 proj_mat = glm::perspective(glm::radians(45.f), aspect_ratio, 0.1f,
                                        100.f);
  proj_mat[1][1] *= -1;

  glm::vec3 light_pos = glm::vec3(0.f, 1.9f, 0.f);

  float shadow_tex_aspect_ratio = static_cast<float>(kShadowTextureWidth) /
      static_cast<float>(kShadowTextureHeight);

  glm::mat4 pos_z_view_mat =
      glm::rotate(glm::mat4(1.f), kPi, glm::vec3(0.f, 1.f, 0.f)) *
          glm::translate(glm::mat4(1.f), -light_pos);
  glm::mat4 shadow_proj_mat = glm::perspective(glm::radians(90.f),
                                               shadow_tex_aspect_ratio,
                                               kShadowPassNearPlane,
                                               kShadowPassFarPlane);
  shadow_proj_mat[1][1] *= -1;

  // Cubemap faces are in left-handed coordinates. E.g. +x is to the right
  // of +z in a cubemap while +x is to the left of +z in Vulkan.
  std::vector<glm::mat4> shadow_view_mats(6);
  shadow_view_mats[0] =  // Right (+x)
      glm::rotate(glm::mat4(1.f), kPi / 2.f, glm::vec3(0.f, 1.f, 0.f)) *
          pos_z_view_mat;
  shadow_view_mats[1] =  // Left (-x)
      glm::rotate(glm::mat4(1.f), -kPi / 2.f, glm::vec3(0.f, 1.f, 0.f)) *
          pos_z_view_mat;
  shadow_view_mats[2] =  // Top (+y)
      glm::rotate(glm::mat4(1.f), -kPi / 2.f, glm::vec3(1.f, 0.f, 0.f)) *
          pos_z_view_mat;
  shadow_view_mats[3] =  // Bottom (-y)
      glm::rotate(glm::mat4(1.f), kPi / 2.f, glm::vec3(1.f, 0.f, 0.f)) *
          pos_z_view_mat;
  shadow_view_mats[4] = pos_z_view_mat;  // Front (+z)
  shadow_view_mats[5] =  // Back (-z)
      glm::rotate(glm::mat4(1.f), kPi, glm::vec3(0.f, 1.f, 0.f)) *
          pos_z_view_mat;

  shadow_mats_.resize(6);
  for (int i = 0; i < shadow_mats_.size(); ++i) {
    shadow_mats_[i] = shadow_proj_mat * shadow_view_mats[i] * model_mat;
  }

  vert_ubo_data.model_mat = model_mat;
  vert_ubo_data.mvp_mat = proj_mat * view_mat * model_mat;

  vert_ubo_buffers_.resize(swap_chain_images_.size());
  vert_ubo_buffers_memory_.resize(swap_chain_images_.size());

  VkDeviceSize vert_ubo_buffer_size = sizeof(VertexShaderUbo);

  for (size_t i = 0; i < swap_chain_images_.size(); i++) {
    VkBufferCreateInfo vert_ubo_buffer_info = {};
    vert_ubo_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vert_ubo_buffer_info.size = vert_ubo_buffer_size;
    vert_ubo_buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    vert_ubo_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    CreateBuffer(&vert_ubo_buffer_info,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 physical_device_, device_, &vert_ubo_buffers_[i],
                 &vert_ubo_buffers_memory_[i]);

    VertexShaderUbo* ubo_ptr;
    vkMapMemory(device_, vert_ubo_buffers_memory_[i], 0, vert_ubo_buffer_size,
                0, reinterpret_cast<void**>(&ubo_ptr));
    *ubo_ptr = vert_ubo_data;
    vkUnmapMemory(device_, vert_ubo_buffers_memory_[i]);

    VkDescriptorBufferInfo descriptor_buffer_info = {};
    descriptor_buffer_info.buffer = vert_ubo_buffers_[i];
    descriptor_buffer_info.offset = 0;
    descriptor_buffer_info.range = vert_ubo_buffer_size;

    VkWriteDescriptorSet descriptor_write = {};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = descriptor_sets_[i];
    descriptor_write.dstBinding = 0;
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_write.descriptorCount = 1;
    descriptor_write.pBufferInfo = &descriptor_buffer_info;

    vkUpdateDescriptorSets(device_, 1, &descriptor_write, 0, nullptr);
  }

  struct Material {
    glm::vec4 ambient_color;
    glm::vec4 diffuse_color;
  };

  struct FragmentShaderUbo {
    glm::vec4 light_pos;
    float shadow_near_plane;
    float shadow_far_plane;
    glm::vec2 pad;
    Material materials[20];
  } frag_ubo_data;

  frag_ubo_data.light_pos = glm::vec4(light_pos, 0.f);
  frag_ubo_data.shadow_near_plane = kShadowPassNearPlane;
  frag_ubo_data.shadow_far_plane = kShadowPassFarPlane;

  for (int i = 0; i < model_.materials.size(); ++i) {
    frag_ubo_data.materials[i].ambient_color =
        glm::vec4(model_.materials[i].ambient_color, 0.f);
    frag_ubo_data.materials[i].diffuse_color =
        glm::vec4(model_.materials[i].diffuse_color, 0.f);
  }

  frag_ubo_buffers_.resize(swap_chain_images_.size());
  frag_ubo_buffers_memory_.resize(swap_chain_images_.size());

  VkDeviceSize frag_ubo_buffer_size = sizeof(FragmentShaderUbo);

  for (size_t i = 0; i < swap_chain_images_.size(); i++) {
    VkBufferCreateInfo vert_ubo_buffer_info = {};
    vert_ubo_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vert_ubo_buffer_info.size = frag_ubo_buffer_size;
    vert_ubo_buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    vert_ubo_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    CreateBuffer(&vert_ubo_buffer_info,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 physical_device_, device_, &frag_ubo_buffers_[i],
                 &frag_ubo_buffers_memory_[i]);

    FragmentShaderUbo* ubo_ptr;
    vkMapMemory(device_, frag_ubo_buffers_memory_[i], 0, frag_ubo_buffer_size,
                0, reinterpret_cast<void**>(&ubo_ptr));
    *ubo_ptr = frag_ubo_data;
    vkUnmapMemory(device_, frag_ubo_buffers_memory_[i]);

    VkDescriptorBufferInfo descriptor_buffer_info = {};
    descriptor_buffer_info.buffer = frag_ubo_buffers_[i];
    descriptor_buffer_info.offset = 0;
    descriptor_buffer_info.range = frag_ubo_buffer_size;

    VkWriteDescriptorSet descriptor_write = {};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = descriptor_sets_[i];
    descriptor_write.dstBinding = 1;
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_write.descriptorCount = 1;
    descriptor_write.pBufferInfo = &descriptor_buffer_info;

    vkUpdateDescriptorSets(device_, 1, &descriptor_write, 0, nullptr);
  }

  VkPhysicalDeviceProperties phys_device_props = {};
  vkGetPhysicalDeviceProperties(physical_device_, &phys_device_props);

  VkSamplerCreateInfo sampler_info = {};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_LINEAR;
  sampler_info.minFilter = VK_FILTER_LINEAR;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.anisotropyEnable = VK_TRUE;
  sampler_info.maxAnisotropy = phys_device_props.limits.maxSamplerAnisotropy;
  sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler_info.unnormalizedCoordinates = VK_FALSE;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

  if (vkCreateSampler(device_, &sampler_info, nullptr, &shadow_texture_sampler_)
          != VK_SUCCESS) {
    std::cerr << "Could not create shadow texture sampler." << std::endl;
    return false;
  }

  for (int i = 0; i < swap_chain_images_.size(); i++) {
    VkDescriptorImageInfo image_info = {};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = shadow_frame_resources_[i].shadow_texture_view;
    image_info.sampler = shadow_texture_sampler_;

    VkWriteDescriptorSet descriptor_write = {};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = descriptor_sets_[i];
    descriptor_write.dstBinding = 2;
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_write.descriptorCount = 1;
    descriptor_write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(device_, 1, &descriptor_write, 0, nullptr);
  }

  return true;
}

bool App::CreateVertexBuffers() {
  VkDeviceSize pos_buffer_size =
      sizeof(glm::vec3) * model_.positions.size();

  VkBufferCreateInfo pos_buffer_info = {};
  pos_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  pos_buffer_info.size = pos_buffer_size;
  pos_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  pos_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  CreateBuffer(&pos_buffer_info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
               physical_device_, device_, &position_buffer_,
               &position_buffer_memory_);

  UploadDataToBuffer(model_.positions.data(), pos_buffer_size,
                     position_buffer_);

  VkDeviceSize normal_buffer_size =
      sizeof(glm::vec3) * model_.normals.size();

  VkBufferCreateInfo normal_buffer_info = {};
  normal_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  normal_buffer_info.size = normal_buffer_size;
  normal_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  normal_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  CreateBuffer(&normal_buffer_info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
               physical_device_, device_, &normal_buffer_,
               &normal_buffer_memory_);

  UploadDataToBuffer(model_.normals.data(), normal_buffer_size,
                     normal_buffer_);

  VkDeviceSize mtl_idx_buffer_size =
      sizeof(uint32_t) * model_.material_indices.size();

  VkBufferCreateInfo mtl_idx_buffer_info = {};
  mtl_idx_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  mtl_idx_buffer_info.size = mtl_idx_buffer_size;
  mtl_idx_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  mtl_idx_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  CreateBuffer(&mtl_idx_buffer_info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
               physical_device_, device_, &material_idx_buffer_,
               &material_idx_buffer_memory_);

  UploadDataToBuffer(model_.material_indices.data(), mtl_idx_buffer_size,
                     material_idx_buffer_);

  VkDeviceSize index_buffer_size =
      sizeof(uint16_t) * model_.index_buffer.size();

  VkBufferCreateInfo index_buffer_info = {};
  index_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  index_buffer_info.size = index_buffer_size;
  index_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  index_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  CreateBuffer(&index_buffer_info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
               physical_device_, device_, &index_buffer_,
               &index_buffer_memory_);

  UploadDataToBuffer(model_.index_buffer.data(), index_buffer_size,
                     index_buffer_);

  return true;
}

void App::UploadDataToBuffer(void* data, VkDeviceSize size, VkBuffer buffer) {
  VkCommandBufferAllocateInfo command_buffer_info = {};
  command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_info.commandPool = command_pool_;
  command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  command_buffer_info.commandBufferCount = 1;

  VkCommandBuffer command_buffer;
  vkAllocateCommandBuffers(device_, &command_buffer_info, &command_buffer);

  VkCommandBufferBeginInfo commands_begin_info = {};
  commands_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  commands_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(command_buffer, &commands_begin_info);

  VkBuffer staging_buffer;
  VkDeviceMemory staging_buffer_memory;

  VkBufferCreateInfo staging_buffer_info = {};
  staging_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  staging_buffer_info.size = size;
  staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  staging_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  CreateBuffer(&staging_buffer_info,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               physical_device_, device_, &staging_buffer,
               &staging_buffer_memory);

  void* staging_ptr;
  vkMapMemory(device_, staging_buffer_memory, 0, size, 0, &staging_ptr);
  memcpy(staging_ptr, data, static_cast<size_t>(size));
  vkUnmapMemory(device_, staging_buffer_memory);

  VkBufferCopy copy_info = {};
  copy_info.size = size;
  vkCmdCopyBuffer(command_buffer, staging_buffer, buffer, 1, &copy_info);

  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo commands_submit_info = {};
  commands_submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  commands_submit_info.commandBufferCount = 1;
  commands_submit_info.pCommandBuffers = &command_buffer;

  vkQueueSubmit(graphics_queue_, 1, &commands_submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(graphics_queue_);

  vkDestroyBuffer(device_, staging_buffer, nullptr);
  vkFreeMemory(device_, staging_buffer_memory, nullptr);

  vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
}

bool App::RecordCommandBuffers() {
  for (int i = 0; i < command_buffers_.size(); ++i) {
    VkCommandBuffer command_buffer = command_buffers_[i];

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
      std::cerr << "Could not begin command buffer." << std::endl;
      return false;
    }

    RecordShadowPassCommands(command_buffer, i);

    TransitionShadowTextureForShaderRead(command_buffer, i);

    RecordScenePassCommands(command_buffer, i);

    TransitionShadowTextureForRendering(command_buffer, i);

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
      std::cerr << "Could not end command buffer." << std::endl;
      return false;
    }
  }
  return true;
}

void App::RecordShadowPassCommands(VkCommandBuffer command_buffer,
                                   int frame_index) {
  ShadowPassFrameResource& frame = shadow_frame_resources_[frame_index];

  for (int i = 0; i < frame.depth_framebuffers.size(); ++i) {
    VkClearValue clear_value = {};
    clear_value.depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = shadow_render_pass_;
    render_pass_begin_info.framebuffer = frame.depth_framebuffers[i];
    render_pass_begin_info.renderArea.offset = {0, 0};
    render_pass_begin_info.renderArea.extent = {
      kShadowTextureWidth, kShadowTextureHeight
    };
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_value;

    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info,
                          VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      shadow_pipeline_);

    vkCmdPushConstants(command_buffer, shadow_pipeline_layout_,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4),
                       &shadow_mats_[i]);

    VkBuffer vertex_buffers[] = { position_buffer_ };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

    vkCmdBindIndexBuffer(command_buffer, index_buffer_, 0,
                         VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(command_buffer, model_.index_buffer.size(), 1, 0, 0, 0);

    vkCmdEndRenderPass(command_buffer);
  }
}

void App::TransitionShadowTextureForShaderRead(VkCommandBuffer command_buffer,
                                              int frame_index) {
  ShadowPassFrameResource& frame = shadow_frame_resources_[frame_index];

  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = frame.shadow_texture;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 6;

  vkCmdPipelineBarrier(command_buffer,
                       VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                       0, nullptr, 1, &barrier);
}

void App::RecordScenePassCommands(VkCommandBuffer command_buffer,
                                  int frame_index) {
  VkClearValue clear_values[2] = {};
  clear_values[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
  clear_values[1].depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo render_pass_begin_info = {};
  render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_begin_info.renderPass = render_pass_;
  render_pass_begin_info.framebuffer = swap_chain_framebuffers_[frame_index];
  render_pass_begin_info.renderArea.offset = {0, 0};
  render_pass_begin_info.renderArea.extent = swap_chain_extent_;
  render_pass_begin_info.clearValueCount = 2;
  render_pass_begin_info.pClearValues = clear_values;

  vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info,
                        VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline_);

  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline_layout_, 0, 1,
                          &descriptor_sets_[frame_index], 0, nullptr);

  VkBuffer vertex_buffers[] = {
    position_buffer_, normal_buffer_, material_idx_buffer_
  };
  VkDeviceSize offsets[] = { 0, 0, 0 };
  vkCmdBindVertexBuffers(command_buffer, 0, 3, vertex_buffers, offsets);

  vkCmdBindIndexBuffer(command_buffer, index_buffer_, 0,
                        VK_INDEX_TYPE_UINT16);

  vkCmdDrawIndexed(command_buffer, model_.index_buffer.size(), 1, 0, 0, 0);

  vkCmdEndRenderPass(command_buffer);
}

void App::TransitionShadowTextureForRendering(VkCommandBuffer command_buffer,
                                              int frame_index) {
  ShadowPassFrameResource& frame = shadow_frame_resources_[frame_index];

  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = frame.shadow_texture;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 6;

  vkCmdPipelineBarrier(command_buffer,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);
}

bool App::CreateSyncObjects() {
  image_ready_semaphores_.resize(kMaxFramesInFlight);
  render_complete_semaphores_.resize(kMaxFramesInFlight);
  frame_ready_fences_.resize(kMaxFramesInFlight);
  image_rendered_fences_.resize(swap_chain_images_.size(), VK_NULL_HANDLE);

  VkSemaphoreCreateInfo semaphore_info = {};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info = {};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (int i = 0; i < kMaxFramesInFlight; i++) {
    if (vkCreateSemaphore(device_, &semaphore_info, nullptr,
                          &image_ready_semaphores_[i]) != VK_SUCCESS ||
        vkCreateSemaphore(device_, &semaphore_info, nullptr,
                          &render_complete_semaphores_[i]) != VK_SUCCESS ||
        vkCreateFence(device_, &fence_info, nullptr,
                      &frame_ready_fences_[i]) != VK_SUCCESS) {
      std::cerr << "Could not create sync objects." << std::endl;
      return false;
    }
  }
  return true;
}

void App::Destroy() {
  for (int i = 0; i < kMaxFramesInFlight; i++) {
    vkDestroySemaphore(device_, image_ready_semaphores_[i], nullptr);
    vkDestroySemaphore(device_, render_complete_semaphores_[i], nullptr);
    vkDestroyFence(device_, frame_ready_fences_[i], nullptr);
  }

  DestroyVertexBuffers();

  DestroyDescriptorSets();

  DestroyCommandBuffers();

  DestroyCommandPool();

  DestroyShadowPassResources();

  DestroyScenePassResources();

  DestroySwapChain();

  vkDestroyDevice(device_, nullptr);
  vkDestroySurfaceKHR(instance_, surface_, nullptr);
  DestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
  vkDestroyInstance(instance_, nullptr);

  glfwDestroyWindow(window_);

  glfwTerminate();
}

void App::DestroyVertexBuffers() {
  vkDestroyBuffer(device_, index_buffer_, nullptr);
  vkFreeMemory(device_, index_buffer_memory_, nullptr);

  vkDestroyBuffer(device_, material_idx_buffer_, nullptr);
  vkFreeMemory(device_, material_idx_buffer_memory_, nullptr);

  vkDestroyBuffer(device_, normal_buffer_, nullptr);
  vkFreeMemory(device_, normal_buffer_memory_, nullptr);

  vkDestroyBuffer(device_, position_buffer_, nullptr);
  vkFreeMemory(device_, position_buffer_memory_, nullptr);
}

void App::DestroyDescriptorSets() {
  vkDestroySampler(device_, shadow_texture_sampler_, nullptr);

  for (int i = 0; i < swap_chain_images_.size(); ++i) {
    vkDestroyBuffer(device_, frag_ubo_buffers_[i], nullptr);
    vkFreeMemory(device_, frag_ubo_buffers_memory_[i], nullptr);
  }
  frag_ubo_buffers_.clear();
  frag_ubo_buffers_memory_.clear();

  for (int i = 0; i < swap_chain_images_.size(); ++i) {
    vkDestroyBuffer(device_, vert_ubo_buffers_[i], nullptr);
    vkFreeMemory(device_, vert_ubo_buffers_memory_[i], nullptr);
  }
  vert_ubo_buffers_.clear();
  vert_ubo_buffers_memory_.clear();

  vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
  descriptor_sets_.clear();
}

void App::DestroyCommandBuffers() {
  vkFreeCommandBuffers(device_, command_pool_, command_buffers_.size(),
                       command_buffers_.data());
  command_buffers_.clear();
}

void App::DestroyCommandPool() {
  vkDestroyCommandPool(device_, command_pool_, nullptr);
}

void App::DestroyShadowPassResources() {
  for (ShadowPassFrameResource& frame : shadow_frame_resources_) {
    vkDestroyImageView(device_, frame.shadow_texture_view, nullptr);
    for (VkFramebuffer framebuffer : frame.depth_framebuffers) {
      vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    for (VkImageView image_view : frame.depth_framebuffer_views) {
      vkDestroyImageView(device_, image_view, nullptr);
    }
    vkDestroyImage(device_, frame.shadow_texture, nullptr);
    vkFreeMemory(device_, frame.shadow_texture_memory, nullptr);
  }
  shadow_frame_resources_.clear();

  vkDestroyPipeline(device_, shadow_pipeline_, nullptr);
  vkDestroyPipelineLayout(device_, shadow_pipeline_layout_, nullptr);
  vkDestroyDescriptorSetLayout(device_, shadow_descriptor_layout_, nullptr);
  vkDestroyRenderPass(device_, shadow_render_pass_, nullptr);
}

void App::DestroyScenePassResources() {
  for (const auto& framebuffer : swap_chain_framebuffers_) {
    vkDestroyFramebuffer(device_, framebuffer, nullptr);
  }
  swap_chain_framebuffers_.clear();

  vkDestroyImageView(device_, color_image_view_, nullptr);
  vkDestroyImage(device_, color_image_, nullptr);
  vkFreeMemory(device_, color_image_memory_, nullptr);

  vkDestroyImageView(device_, depth_image_view_, nullptr);
  vkDestroyImage(device_, depth_image_, nullptr);
  vkFreeMemory(device_, depth_image_memory_, nullptr);

  vkDestroyPipeline(device_, pipeline_, nullptr);
  vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
  vkDestroyDescriptorSetLayout(device_, descriptor_set_layout_, nullptr);
  vkDestroyRenderPass(device_, render_pass_, nullptr);
}

void App::DestroySwapChain() {
  for (const auto& image_view : swap_chain_image_views_) {
    vkDestroyImageView(device_, image_view, nullptr);
  }
  swap_chain_image_views_.clear();

  vkDestroySwapchainKHR(device_, swap_chain_, nullptr);
}

void App::MainLoop() {
  while (!glfwWindowShouldClose(window_)) {
    glfwPollEvents();
    if (!DrawFrame())
      break;
  }
  vkDeviceWaitIdle(device_);
}

bool App::DrawFrame() {
  vkWaitForFences(device_, 1, &frame_ready_fences_[current_frame_], VK_TRUE,
                  UINT64_MAX);

  uint32_t image_index;
  VkResult result = vkAcquireNextImageKHR(
      device_, swap_chain_, UINT64_MAX,
      image_ready_semaphores_[current_frame_], VK_NULL_HANDLE, &image_index);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    RecreateSwapChain();
    return true;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    std::cerr << "Could not acquire image." << std::endl;
    return false;
  }

  if (image_rendered_fences_[image_index] != VK_NULL_HANDLE) {
    vkWaitForFences(device_, 1, &image_rendered_fences_[image_index], VK_TRUE,
                    UINT64_MAX);
  }
  image_rendered_fences_[image_index] = frame_ready_fences_[current_frame_];

  vkResetFences(device_, 1, &frame_ready_fences_[current_frame_]);

  VkSemaphore submit_wait_semaphores[] = {
    image_ready_semaphores_[current_frame_]
  };
  VkPipelineStageFlags submit_wait_stages[] = {
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  };

  VkSemaphore submit_signal_semaphores[] = {
    render_complete_semaphores_[current_frame_]
  };

  VkSubmitInfo queue_submit_info = {};
  queue_submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  queue_submit_info.waitSemaphoreCount = 1;
  queue_submit_info.pWaitSemaphores = submit_wait_semaphores;
  queue_submit_info.pWaitDstStageMask = submit_wait_stages;
  queue_submit_info.commandBufferCount = 1;
  queue_submit_info.pCommandBuffers = &command_buffers_[image_index];
  queue_submit_info.signalSemaphoreCount = 1;
  queue_submit_info.pSignalSemaphores = submit_signal_semaphores;

  if (vkQueueSubmit(graphics_queue_, 1, &queue_submit_info,
                    frame_ready_fences_[current_frame_]) != VK_SUCCESS) {
    std::cerr << "Could not submit to queue." << std::endl;
    return false;
  }

  VkSemaphore present_wait_semaphores[] = {
    render_complete_semaphores_[current_frame_]
  };

  VkSwapchainKHR swap_chains[] = { swap_chain_ };

  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = present_wait_semaphores;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swap_chain_;
  present_info.pImageIndices = &image_index;

  result = vkQueuePresentKHR(present_queue_, &present_info);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      framebuffer_resized_) {
    framebuffer_resized_ = false;
    RecreateSwapChain();
    return true;
  } else if (result != VK_SUCCESS) {
    std::cerr << "Could not present to swap chain." << std::endl;
    return false;
  }

  current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;

  return true;
}

bool App::RecreateSwapChain() {
  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(window_, &width, &height);
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(window_, &width, &height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(device_);

  DestroyDescriptorSets();

  DestroyCommandBuffers();

  DestroyShadowPassResources();

  DestroyScenePassResources();

  DestroySwapChain();

  if (!CreateSwapChain())
    return false;

  if (!CreateScenePassResources())
    return false;

  if (!CreateShadowPassResources())
    return false;

  if (!CreateDescriptorSets())
    return false;

  if (!CreateCommandBuffers())
    return false;

  if (!RecordCommandBuffers())
    return false;

  image_rendered_fences_.resize(swap_chain_images_.size(), VK_NULL_HANDLE);

  return true;
}