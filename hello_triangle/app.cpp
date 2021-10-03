#include "app.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

const char* kRequiredValidationLayers[] = {
  "VK_LAYER_KHRONOS_validation"
};

const char* kRequiredDeviceExtensions[] = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

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

VkPhysicalDevice ChoosePhysicalDevice(VkInstance instance, 
                                      VkSurfaceKHR surface) {
  uint32_t count = 0;
  vkEnumeratePhysicalDevices(instance, &count, nullptr);
  if (count == 0)
    return VK_NULL_HANDLE;

  std::vector<VkPhysicalDevice> devices(count);
  vkEnumeratePhysicalDevices(instance, &count, devices.data());

  for (const auto& device : devices) {
    if (IsPhysicalDeviceSuitable(device, surface))
      return device;
  }
  
  return VK_NULL_HANDLE;
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
  shader_module_info.pCode = reinterpret_cast<const uint32_t*>(shader_data.data());

  VkShaderModule shader_module;
  if (vkCreateShaderModule(device, &shader_module_info, nullptr, &shader_module) 
          != VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }
  return shader_module;
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

  physical_device_ = ChoosePhysicalDevice(instance_, surface_);
  if (physical_device_ == VK_NULL_HANDLE) {
    std::cerr << "Could not find suitable physical device." << std::endl;
    return false;
  }

  QueueFamilyIndices queue_indices = FindQueueFamilyIndices(physical_device_,
                                                            surface_);
  std::set<uint32_t> unique_queue_indices = {
    queue_indices.graphics_family_index.value(), 
    queue_indices.present_family_index.value()
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

  VkDeviceCreateInfo device_info = {};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
  device_info.pQueueCreateInfos = queue_infos.data();
  device_info.pEnabledFeatures = &device_features;
  device_info.enabledExtensionCount = 
      static_cast<uint32_t>(device_extensions.size());
  device_info.ppEnabledExtensionNames = device_extensions.data();
  device_info.enabledLayerCount = 
      static_cast<uint32_t>(validation_layers.size());
  device_info.ppEnabledLayerNames = validation_layers.data();

  if (vkCreateDevice(physical_device_, &device_info, nullptr, &device_) 
          != VK_SUCCESS) {
    std::cerr << "Could not create device." << std::endl;
    return false;
  }

  vkGetDeviceQueue(device_, queue_indices.graphics_family_index.value(), 0, 
                   &graphics_queue_);
  vkGetDeviceQueue(device_, queue_indices.present_family_index.value(), 0, 
                   &present_queue_);

  SwapChainSupport swap_chain_support = QuerySwapChainSupport(physical_device_,
                                                              surface_);

  VkSurfaceFormatKHR surface_format = 
      ChooseSurfaceFormat(swap_chain_support.formats);
  VkPresentModeKHR present_mode = 
      ChoosePresentMode(swap_chain_support.present_modes);

  swap_chain_image_format_ = surface_format.format;
  swap_chain_extent_ = ChooseSwapChainExtent(swap_chain_support.capabilities, 
                                             window_);

  uint32_t swap_chain_image_count = 
      std::min(swap_chain_support.capabilities.minImageCount + 1, 
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

  if (queue_indices.graphics_family_index.value() 
          != queue_indices.present_family_index.value()) {
    uint32_t indices[] = {
       queue_indices.graphics_family_index.value(),
       queue_indices.present_family_index.value()
    };        
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

  for (const auto& image : swap_chain_images_) {
    VkImageViewCreateInfo image_view_info = {};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = swap_chain_image_format_;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    if (vkCreateImageView(device_, &image_view_info, nullptr, &image_view) 
            != VK_SUCCESS) {
      std::cerr << "Could not create swap chain image view." << std::endl;
      return false;
    }
    swap_chain_image_views_.push_back(image_view);
  }

  VkAttachmentDescription color_attachment = {};
  color_attachment.format = swap_chain_image_format_;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_ref = {};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;

  VkSubpassDependency subpass_dep = {};
  subpass_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  subpass_dep.dstSubpass = 0;
  subpass_dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpass_dep.srcAccessMask = 0;
  subpass_dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpass_dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo render_pass_info = {};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &color_attachment;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &subpass_dep;

  if (vkCreateRenderPass(device_, &render_pass_info, nullptr, &render_pass_) 
          != VK_SUCCESS) {
    std::cerr << "Could not create render pass." << std::endl;
    return false;
  }

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

  VkPipelineShaderStageCreateInfo shader_stages[] = {
    vert_shader_info, 
    frag_shader_info
  };

  VkVertexInputBindingDescription vertex_binding = {};
  vertex_binding.binding = 0;
  vertex_binding.stride = sizeof(float) * 2;
  vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription attribute_desc = {};
  attribute_desc.binding = 0;
  attribute_desc.location = 0;
  attribute_desc.format = VK_FORMAT_R32G32_SFLOAT;
  attribute_desc.offset = 0;

  VkPipelineVertexInputStateCreateInfo vertex_input = {};
  vertex_input.sType = 
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input.vertexBindingDescriptionCount = 1;
  vertex_input.pVertexBindingDescriptions = &vertex_binding;
  vertex_input.vertexAttributeDescriptionCount = 1;
  vertex_input.pVertexAttributeDescriptions = &attribute_desc;

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

void App::Destroy() {
  vkDestroyPipeline(device_, pipeline_, nullptr);
  vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
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
  }
}