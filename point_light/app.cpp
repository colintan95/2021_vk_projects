#include "app.h"

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

#include "utils/model.h"

namespace {

const char* kRequiredValidationLayers[] = {
  "VK_LAYER_KHRONOS_validation"
};

const char* kRequiredDeviceExtensions[] = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

constexpr int kMaxFramesInFlight = 3;

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

struct QueueFamilyIndices {
  std::optional<uint32_t> graphics_family_index;
  std::optional<uint32_t> present_family_index;
};

bool FoundQueueFamilies(const QueueFamilyIndices& indices) {
  return indices.graphics_family_index.has_value() &&
      indices.present_family_index.has_value();
}

QueueFamilyIndices FindQueueFamilyIndices(VkPhysicalDevice device,
                                          VkSurfaceKHR surface) {
  QueueFamilyIndices indices = {};

  uint32_t count;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);

  std::vector<VkQueueFamilyProperties> families(count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

  for (int i = 0; i < families.size(); ++i) {
    VkQueueFamilyProperties family = families[i];

    if ((family.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 1)
      indices.graphics_family_index = i;

    VkBool32 present_support = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);

    if (present_support)
      indices.present_family_index = i;

    if (FoundQueueFamilies(indices))
      return indices;
  }

  return indices;
}

bool SupportsRequiredDeviceExtensions(VkPhysicalDevice device) {
  uint32_t count;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);

  std::vector<VkExtensionProperties> available_extensions(count);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count,
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

struct SwapChainSupport {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

SwapChainSupport QuerySwapChainSupport(VkPhysicalDevice device,
                                       VkSurfaceKHR surface) {
  SwapChainSupport support = {};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                            &support.capabilities);

  uint32_t format_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);

  support.formats.resize(format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count,
                                       support.formats.data());

  uint32_t present_mode_count;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
                                            &present_mode_count, nullptr);

  support.present_modes.resize(present_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
                                            &present_mode_count,
                                            support.present_modes.data());

  return support;
}

bool IsPhysicalDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
  QueueFamilyIndices queue_indices = FindQueueFamilyIndices(device, surface);
  if (!FoundQueueFamilies(queue_indices))
    return false;

  if (!SupportsRequiredDeviceExtensions(device))
    return false;

  SwapChainSupport swap_chain_support = QuerySwapChainSupport(device, surface);
  if (swap_chain_support.formats.empty() ||
      swap_chain_support.present_modes.empty())
    return false;

  VkPhysicalDeviceFeatures features;
  vkGetPhysicalDeviceFeatures(device, &features);
  if (!features.samplerAnisotropy)
    return false;

  return true;
}

VkSurfaceFormatKHR ChooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats) {
  for (const auto& format : formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      return format;
  }
  return formats[0];
}

VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
  for (const auto& mode : modes) {
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
                             VkPhysicalDevice device) {
  for (const auto& format : formats) {
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(device, format, &properties);

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

VkFormat FindDepthFormat(VkPhysicalDevice device) {
  std::vector<VkFormat> formats = {
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT
  };
  return FindSupportedFormat(formats, VK_IMAGE_TILING_OPTIMAL,
                             VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
                             device);
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

VkShaderModule CreateShaderModule(const std::vector<char>& shader_data,
                                  VkDevice device) {
  VkShaderModuleCreateInfo shader_module_info = {};
  shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_module_info.codeSize = shader_data.size();
  shader_module_info.pCode =
      reinterpret_cast<const uint32_t*>(shader_data.data());

  VkShaderModule shader_module;
  if (vkCreateShaderModule(device, &shader_module_info, nullptr, &shader_module)
          != VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }
  return shader_module;
}

std::optional<uint32_t> FindMemoryTypeIndex(
    uint32_t memory_type_bits,
    VkMemoryPropertyFlags mem_properties,
    VkPhysicalDevice physical_device) {
  VkPhysicalDeviceMemoryProperties phys_device_mem_props;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &phys_device_mem_props);

  for (int i = 0; i < phys_device_mem_props.memoryTypeCount; i++) {
    if ((memory_type_bits & (1 << i)) &&
        (phys_device_mem_props.memoryTypes[i].propertyFlags & mem_properties)
             == mem_properties) {
      return i;
    }
  }
  return std::nullopt;
}

bool CreateImage(uint32_t width, uint32_t height, VkFormat format,
                 VkImageTiling tiling, VkImageUsageFlags usage,
                 VkMemoryPropertyFlags mem_properties,
                 VkPhysicalDevice physical_device,VkDevice device,
                 VkImage* image, VkDeviceMemory* memory) {
  VkImageCreateInfo image_info = {};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = width;
  image_info.extent.height = height;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.format = format;
  image_info.tiling = tiling;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage = usage;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateImage(device, &image_info, nullptr, image) != VK_SUCCESS)
    return false;

  VkMemoryRequirements mem_requirements;
  vkGetImageMemoryRequirements(device, *image, &mem_requirements);

  std::optional<uint32_t> memory_type_index =
      FindMemoryTypeIndex(mem_requirements.memoryTypeBits, mem_properties,
                          physical_device);
  if (!memory_type_index.has_value())
    return false;

  VkMemoryAllocateInfo mem_alloc_info = {};
  mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_alloc_info.allocationSize = mem_requirements.size;
  mem_alloc_info.memoryTypeIndex = memory_type_index.value();

  if (vkAllocateMemory(device, &mem_alloc_info, nullptr, memory) != VK_SUCCESS)
    return false;

  vkBindImageMemory(device, *image, *memory, 0);

  return true;
}

bool CreateImageView(VkImage image, VkFormat format,
                     VkImageAspectFlags aspect_flags, VkDevice device,
                     VkImageView* image_view) {
  VkImageViewCreateInfo image_view_info = {};
  image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_info.image = image;
  image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  image_view_info.format = format;
  image_view_info.subresourceRange.aspectMask = aspect_flags;
  image_view_info.subresourceRange.baseMipLevel = 0;
  image_view_info.subresourceRange.levelCount = 1;
  image_view_info.subresourceRange.baseArrayLayer = 0;
  image_view_info.subresourceRange.layerCount = 1;

  if (vkCreateImageView(device, &image_view_info, nullptr, image_view)
          != VK_SUCCESS) {
    return false;
  }
  return true;
}

bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags mem_properties,
                  VkPhysicalDevice physical_device, VkDevice device,
                  VkBuffer* buffer, VkDeviceMemory* memory) {
  VkBufferCreateInfo buffer_info = {};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = size;
  buffer_info.usage = usage;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(device, &buffer_info, nullptr, buffer) != VK_SUCCESS)
    return false;

  VkMemoryRequirements mem_requirements;
  vkGetBufferMemoryRequirements(device, *buffer, &mem_requirements);

  std::optional<uint32_t> memory_type_index =
      FindMemoryTypeIndex(mem_requirements.memoryTypeBits, mem_properties,
                          physical_device);
  if (!memory_type_index.has_value())
    return false;

  VkMemoryAllocateInfo mem_alloc_info = {};
  mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_alloc_info.allocationSize = mem_requirements.size;
  mem_alloc_info.memoryTypeIndex = memory_type_index.value();

  if (vkAllocateMemory(device, &mem_alloc_info, nullptr, memory)
          != VK_SUCCESS) {
    return false;
  }
  vkBindBufferMemory(device, *buffer, *memory, 0);

  return true;
}

}  // namespace

bool App::Init() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  window_ = glfwCreateWindow(800, 600, "Vulkan Application", nullptr, nullptr);
  glfwSetWindowUserPointer(window_, this);
  glfwSetFramebufferSizeCallback(window_, GlfwFramebufferResized);

  if (!InitInstanceAndSurface())
    return false;

  if (!ChoosePhysicalDevice())
    return false;

  if (!CreateDevice())
    return false;

  if (!CreateSwapChain())
    return false;

  if (!CreateRenderPass())
    return false;

  if (!CreatePipeline())
    return false;

  if (!CreateFramebuffers())
    return false;

  if (!CreateCommandPool())
    return false;

  if (!CreateCommandBuffers())
    return false;

  if (!CreateDescriptorPool())
    return false;

  if (!CreateDescriptorSets())
    return false;

  if (!LoadModel())
    return false;

  if (!InitDescriptors())
    return false;

  if (!InitBuffers())
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
  uint32_t count = 0;
  vkEnumeratePhysicalDevices(instance_, &count, nullptr);
  if (count == 0) {
    std::cerr << "Could not find suitable physical device." << std::endl;
    return false;
  }

  std::vector<VkPhysicalDevice> devices(count);
  vkEnumeratePhysicalDevices(instance_, &count, devices.data());

  for (const auto& device : devices) {
    if (IsPhysicalDeviceSuitable(device, surface_)) {
      physical_device_ = device;
      return true;
    }
  }
  std::cerr << "Could not find suitable physical device." << std::endl;
  return false;
}

bool App::CreateDevice() {
  QueueFamilyIndices queue_indices = FindQueueFamilyIndices(physical_device_,
                                                            surface_);
  graphics_queue_index_ = queue_indices.graphics_family_index.value();
  present_queue_index_ = queue_indices.present_family_index.value();

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

  VkPhysicalDeviceFeatures device_features = {};
  device_features.samplerAnisotropy = VK_TRUE;

  std::vector<const char*> device_extensions = GetRequiredDeviceExtensions();
  std::vector<const char*> validation_layers = GetRequiredValidationLayers();

  VkDeviceCreateInfo device_info = {};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
  device_info.pQueueCreateInfos = queue_infos.data();
  device_info.pEnabledFeatures = &device_features;
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

  return true;
}

bool App::CreateSwapChain() {
  SwapChainSupport swap_chain_support = QuerySwapChainSupport(physical_device_,
                                                              surface_);

  VkSurfaceFormatKHR surface_format = ChooseSurfaceFormat(
      swap_chain_support.formats);
  VkPresentModeKHR present_mode = ChoosePresentMode(
      swap_chain_support.present_modes);

  swap_chain_image_format_ = surface_format.format;
  swap_chain_extent_ = ChooseSwapChainExtent(swap_chain_support.capabilities,
                                             window_);

  uint32_t swap_chain_image_count = std::min(
      swap_chain_support.capabilities.minImageCount + 1,
      swap_chain_support.capabilities.maxImageCount);

  VkSwapchainCreateInfoKHR swap_chain_info = {};
  swap_chain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swap_chain_info.surface = surface_;
  swap_chain_info.minImageCount = swap_chain_image_count;
  swap_chain_info.imageFormat = swap_chain_image_format_;
  swap_chain_info.imageColorSpace = surface_format.colorSpace;
  swap_chain_info.imageExtent = swap_chain_extent_;
  swap_chain_info.imageArrayLayers = 1;
  swap_chain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swap_chain_info.preTransform =
      swap_chain_support.capabilities.currentTransform;
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
    if (!CreateImageView(swap_chain_images_[i], swap_chain_image_format_,
                         VK_IMAGE_ASPECT_COLOR_BIT, device_,
                         &swap_chain_image_views_[i]) != VK_SUCCESS) {
      std::cerr << "Could not create swap chain image view." << std::endl;
      return false;
    }
  }
  return true;
}

bool App::CreateRenderPass() {
  VkAttachmentDescription color_attachment = {};
  color_attachment.format = swap_chain_image_format_;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkFormat depth_format = FindDepthFormat(physical_device_);
  if (depth_format == VK_FORMAT_UNDEFINED) {
    std::cerr << "Could not find suitable depth format." << std::endl;
    return false;
  }

  VkAttachmentDescription depth_attachment = {};
  depth_attachment.format = depth_format;
  depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference color_attachment_ref = {};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_attachment_ref = {};
  depth_attachment_ref.attachment = 1;
  depth_attachment_ref.layout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;
  subpass.pDepthStencilAttachment = &depth_attachment_ref;

  VkSubpassDependency subpass_dep = {};
  subpass_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  subpass_dep.dstSubpass = 0;
  subpass_dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  subpass_dep.srcAccessMask = 0;
  subpass_dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  subpass_dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  VkAttachmentDescription attachments[] = {
    color_attachment, depth_attachment
  };

  VkRenderPassCreateInfo render_pass_info = {};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 2;
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
  std::vector<char> vert_shader_data = LoadShaderFile("shader_vert.spv");
  if (vert_shader_data.empty()) {
    std::cerr << "Could not load vert shader file." << std::endl;
    return false;
  }

  std::vector<char> frag_shader_data = LoadShaderFile("shader_frag.spv");
  if (frag_shader_data.empty()) {
    std::cerr << "Could not load frag shader file." << std::endl;
    return false;
  }

  VkShaderModule vert_shader_module = CreateShaderModule(vert_shader_data,
                                                         device_);
  if (vert_shader_module == VK_NULL_HANDLE) {
    std::cerr << "Could not create vert shader module." << std::endl;
    return false;
  }

  VkShaderModule frag_shader_module = CreateShaderModule(frag_shader_data,
                                                         device_);
  if (frag_shader_module == VK_NULL_HANDLE) {
    std::cerr << "Could not create frag shader module." << std::endl;
    return false;
  }

  VkPipelineShaderStageCreateInfo vert_shader_info = {};
  vert_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vert_shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vert_shader_info.module = vert_shader_module;
  vert_shader_info.pName = "main";

  VkPipelineShaderStageCreateInfo frag_shader_info = {};
  frag_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  frag_shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  frag_shader_info.module = frag_shader_module;
  frag_shader_info.pName = "main";

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

  VkDescriptorSetLayoutBinding descriptor_set_bindings[] = {
    vert_ubo_binding, frag_ubo_binding
  };

  VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {};
  descriptor_set_layout_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptor_set_layout_info.bindingCount = 2;
  descriptor_set_layout_info.pBindings = descriptor_set_bindings;

  if (vkCreateDescriptorSetLayout(device_, &descriptor_set_layout_info, nullptr,
                                  &descriptor_set_layout_) != VK_SUCCESS) {
    std::cerr << "Could not create descriptor set." << std::endl;
    return false;
  }

  VkPipelineShaderStageCreateInfo shader_stages[] = {
    vert_shader_info, frag_shader_info
  };

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
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

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

  vkDestroyShaderModule(device_, vert_shader_module, nullptr);
  vkDestroyShaderModule(device_, frag_shader_module, nullptr);

  return true;
}

bool App::CreateFramebuffers() {
  VkFormat depth_format = FindDepthFormat(physical_device_);
  if (depth_format == VK_FORMAT_UNDEFINED) {
    std::cerr << "Could not find suitable depth format." << std::endl;
    return false;
  }

  if (!CreateImage(swap_chain_extent_.width, swap_chain_extent_.height,
                   depth_format, VK_IMAGE_TILING_OPTIMAL,
                   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, physical_device_,
                   device_, &depth_image_, &depth_image_memory_)) {
    std::cerr << "Could not create depth image." << std::endl;
    return false;
  }

  if (!CreateImageView(depth_image_, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT,
                       device_, &depth_image_view_)) {
    std::cerr << "Could not create depth image view." << std::endl;
    return false;
  }

  swap_chain_framebuffers_.resize(swap_chain_images_.size());

  for (int i = 0; i < swap_chain_images_.size(); ++i) {
    VkImageView attachments[] = {
      swap_chain_image_views_[i], depth_image_view_
    };

    VkFramebufferCreateInfo framebuffer_info = {};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = render_pass_;
    framebuffer_info.attachmentCount = 2;
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

bool App::CreateDescriptorPool() {
  VkDescriptorPoolSize pool_size = {};
  pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_size.descriptorCount =
      static_cast<uint32_t>(swap_chain_images_.size()) * 2;

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 1;
  pool_info.pPoolSizes = &pool_size;
  pool_info.maxSets = static_cast<uint32_t>(swap_chain_images_.size());

  if (vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_)
          != VK_SUCCESS) {
    std::cerr << "Could not create descriptor pool." << std::endl;
    return false;
  }
  return true;
}

bool App::CreateDescriptorSets() {
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
  return true;
}

bool App::LoadModel() {
  if (!utils::LoadModel("cornell_box.obj", &model_)) {
    std::cerr << "Could not load model." << std::endl;
    return false;
  }
  return true;
}

bool App::InitDescriptors() {
  struct VertexShaderUbo {
    glm::mat4 model_mat;
    glm::mat4 mvp_mat;
  } vert_ubo_data;

  float aspect_ratio = static_cast<float>(swap_chain_extent_.width) /
      static_cast<float>(swap_chain_extent_.height);

  glm::mat4 model_mat = glm::mat4(1.f);
  glm::mat4 view_mat = glm::translate(glm::mat4(1.f),
                                      glm::vec3(0.f, -1.f, -3.5f));
  glm::mat4 proj_mat = glm::perspective(45.f, aspect_ratio, 0.1f, 100.f);
  proj_mat[1][1] *= -1;

  vert_ubo_data.model_mat = model_mat;
  vert_ubo_data.mvp_mat = proj_mat * view_mat * model_mat;

  vert_ubo_buffers_.resize(swap_chain_images_.size());
  vert_ubo_buffers_memory_.resize(swap_chain_images_.size());

  VkDeviceSize vert_ubo_buffer_size = sizeof(VertexShaderUbo);

  for (size_t i = 0; i < swap_chain_images_.size(); i++) {
    CreateBuffer(vert_ubo_buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
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
    Material materials[20];
  } frag_ubo_data;

  frag_ubo_data.light_pos = glm::vec4(0.f, 1.9f, 0.f, 0.f);

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
    CreateBuffer(frag_ubo_buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
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

  return true;
}

bool App::InitBuffers() {
  VkDeviceSize position_buffer_size =
      sizeof(glm::vec3) * model_.positions.size();

  CreateBuffer(position_buffer_size,
               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, physical_device_,
               device_, &position_buffer_, &position_buffer_memory_);

  UploadDataToBuffer(model_.positions.data(), position_buffer_size,
                     position_buffer_);

  VkDeviceSize normal_buffer_size =
      sizeof(glm::vec3) * model_.normals.size();

  CreateBuffer(normal_buffer_size,
               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, physical_device_,
               device_, &normal_buffer_, &normal_buffer_memory_);

  UploadDataToBuffer(model_.normals.data(), normal_buffer_size,
                     normal_buffer_);

  VkDeviceSize material_idx_buffer_size = sizeof(uint32_t) *
      model_.material_indices.size();

  CreateBuffer(material_idx_buffer_size,
               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, physical_device_,
               device_, &material_idx_buffer_, &material_idx_buffer_memory_);

  UploadDataToBuffer(model_.material_indices.data(), material_idx_buffer_size,
                     material_idx_buffer_);

  VkDeviceSize index_buffer_size =
      sizeof(uint16_t) * model_.index_buffer.size();

  CreateBuffer(index_buffer_size,
               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, physical_device_,
               device_, &index_buffer_, &index_buffer_memory_);

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
  CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
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

    VkClearValue clear_values[2] = {};
    clear_values[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clear_values[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = render_pass_;
    render_pass_begin_info.framebuffer = swap_chain_framebuffers_[i];
    render_pass_begin_info.renderArea.offset = {0, 0};
    render_pass_begin_info.renderArea.extent = swap_chain_extent_;
    render_pass_begin_info.clearValueCount = 2;
    render_pass_begin_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline_);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout_, 0, 1, &descriptor_sets_[i], 0,
                            nullptr);

    VkBuffer vertex_buffers[] = {
      position_buffer_, normal_buffer_, material_idx_buffer_
    };
    VkDeviceSize offsets[] = { 0, 0, 0 };
    vkCmdBindVertexBuffers(command_buffer, 0, 3, vertex_buffers, offsets);

    vkCmdBindIndexBuffer(command_buffer, index_buffer_, 0,
                         VK_INDEX_TYPE_UINT16);

    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdDrawIndexed(command_buffer, model_.index_buffer.size(), 1, 0, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
      std::cerr << "Could not end command buffer." << std::endl;
      return false;
    }
  }
  return true;
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

  vkDestroyBuffer(device_, index_buffer_, nullptr);
  vkFreeMemory(device_, index_buffer_memory_, nullptr);

  vkDestroyBuffer(device_, material_idx_buffer_, nullptr);
  vkFreeMemory(device_, material_idx_buffer_memory_, nullptr);

  vkDestroyBuffer(device_, normal_buffer_, nullptr);
  vkFreeMemory(device_, normal_buffer_memory_, nullptr);

  vkDestroyBuffer(device_, position_buffer_, nullptr);
  vkFreeMemory(device_, position_buffer_memory_, nullptr);

  for (int i = 0; i < swap_chain_images_.size(); ++i) {
    vkDestroyBuffer(device_, frag_ubo_buffers_[i], nullptr);
    vkFreeMemory(device_, frag_ubo_buffers_memory_[i], nullptr);
  }

  for (int i = 0; i < swap_chain_images_.size(); ++i) {
    vkDestroyBuffer(device_, vert_ubo_buffers_[i], nullptr);
    vkFreeMemory(device_, vert_ubo_buffers_memory_[i], nullptr);
  }

  vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
  vkDestroyCommandPool(device_, command_pool_, nullptr);

  for (const auto& framebuffer : swap_chain_framebuffers_) {
    vkDestroyFramebuffer(device_, framebuffer, nullptr);
  }

  vkDestroyImageView(device_, depth_image_view_, nullptr);
  vkDestroyImage(device_, depth_image_, nullptr);
  vkFreeMemory(device_, depth_image_memory_, nullptr);

  vkDestroyPipeline(device_, pipeline_, nullptr);
  vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
  vkDestroyDescriptorSetLayout(device_, descriptor_set_layout_, nullptr);
  vkDestroyRenderPass(device_, render_pass_, nullptr);
  for (const auto& image_view : swap_chain_image_views_) {
    vkDestroyImageView(device_, image_view, nullptr);
  }
  vkDestroySwapchainKHR(device_, swap_chain_, nullptr);
  vkDestroyDevice(device_, nullptr);
  vkDestroySurfaceKHR(instance_, surface_, nullptr);
  DestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
  vkDestroyInstance(instance_, nullptr);

  glfwDestroyWindow(window_);

  glfwTerminate();
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

  VkSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = submit_wait_semaphores;
  submit_info.pWaitDstStageMask = submit_wait_stages;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffers_[image_index];
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = submit_signal_semaphores;

  if (vkQueueSubmit(graphics_queue_, 1, &submit_info,
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

  for (const auto& framebuffer : swap_chain_framebuffers_) {
    vkDestroyFramebuffer(device_, framebuffer, nullptr);
  }
  swap_chain_framebuffers_.clear();

  vkDestroyImageView(device_, depth_image_view_, nullptr);
  vkDestroyImage(device_, depth_image_, nullptr);
  vkFreeMemory(device_, depth_image_memory_, nullptr);

  vkDestroyPipeline(device_, pipeline_, nullptr);
  vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
  vkDestroyDescriptorSetLayout(device_, descriptor_set_layout_, nullptr);
  vkDestroyRenderPass(device_, render_pass_, nullptr);
  for (const auto& image_view : swap_chain_image_views_) {
    vkDestroyImageView(device_, image_view, nullptr);
  }
  swap_chain_image_views_.clear();
  vkDestroySwapchainKHR(device_, swap_chain_, nullptr);

  if (!CreateSwapChain())
    return false;

  if (!CreateRenderPass())
    return false;

  if (!CreatePipeline())
    return false;

  if (!CreateFramebuffers())
    return false;

  if (!CreateDescriptorPool())
    return false;

  if (!CreateDescriptorSets())
    return false;

  if (!InitDescriptors())
    return false;

  if (!CreateCommandBuffers())
    return false;

  if (!RecordCommandBuffers())
    return false;

  image_rendered_fences_.resize(swap_chain_images_.size(), VK_NULL_HANDLE);

  return true;
}