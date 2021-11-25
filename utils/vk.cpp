#include "utils/vk.h"

#include <vulkan/vulkan.h>

#include <fstream>
#include <optional>
#include <vector>

namespace utils {
namespace vk {

namespace {

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

bool AllocateMemoryForResource(VkMemoryPropertyFlags mem_properties,
                               VkMemoryRequirements mem_requirements,
                               VkPhysicalDevice physical_device,
                               VkDevice device, VkDeviceMemory& memory) {
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

  VkMemoryAllocateInfo mem_alloc_info{};
  mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_alloc_info.allocationSize = mem_requirements.size;
  mem_alloc_info.memoryTypeIndex = memory_type_index.value();

  if (vkAllocateMemory(device, &mem_alloc_info, nullptr, &memory) != VK_SUCCESS)
    return false;

  return true;
}

}  // namespace

bool SupportsValidationLayers(const std::vector<const char*>& layers) {
  uint32_t count;
  vkEnumerateInstanceLayerProperties(&count, nullptr);

  std::vector<VkLayerProperties> available_layers(count);
  vkEnumerateInstanceLayerProperties(&count, available_layers.data());

  for (const char* layer_name : layers) {
    bool found = false;
    for (const auto& available_layer : available_layers) {
      if (strcmp(available_layer.layerName, layer_name) == 0) {
        found = true;
        break;
      }
    }
    if (!found)
      return false;
  }

  return true;
}

bool SupportsDeviceExtensions(VkPhysicalDevice physical_device,
                              const std::vector<const char*>& extensions) {
  uint32_t count;
  vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count,
                                       nullptr);

  std::vector<VkExtensionProperties> available_extensions(count);
  vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count,
                                       available_extensions.data());

  for (const char* extension_name : extensions) {
    bool found = false;
    for (const auto& available_extension : available_extensions) {
      if (strcmp(available_extension.extensionName, extension_name) == 0) {
        found = true;
        break;
      }
    }
    if (!found)
      return false;
  }

  return true;
}

bool CreateShaderModulesFromFiles(const std::vector<std::string>& file_paths,
                                  VkDevice device,
                                  std::vector<VkShaderModule>* shader_modules) {
  shader_modules->clear();

  for (const std::string& path : file_paths) {
    std::vector<char> data = LoadShaderFile(path);
    if (data.empty())
      return false;

    VkShaderModuleCreateInfo shader_module_info{};
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

bool CreateImage(const VkImageCreateInfo& image_info,
                 VkMemoryPropertyFlags mem_properties,
                 VkPhysicalDevice physical_device, VkDevice device,
                 VkImage& image, VkDeviceMemory& memory) {
  if (vkCreateImage(device, &image_info, nullptr, &image) != VK_SUCCESS)
    return false;

  VkMemoryRequirements mem_requirements;
  vkGetImageMemoryRequirements(device, image, &mem_requirements);

  if (!AllocateMemoryForResource(mem_properties, mem_requirements,
                                 physical_device, device, memory))
    return false;
  vkBindImageMemory(device, image, memory, 0);

  return true;
}

bool CreateBuffer(const VkBufferCreateInfo& buffer_info,
                  VkMemoryPropertyFlags mem_properties,
                  VkPhysicalDevice physical_device, VkDevice device,
                  VkBuffer& buffer, VkDeviceMemory& memory) {
  if (vkCreateBuffer(device, &buffer_info, nullptr, &buffer) != VK_SUCCESS)
    return false;

  VkMemoryRequirements mem_requirements;
  vkGetBufferMemoryRequirements(device, buffer, &mem_requirements);

 if (!AllocateMemoryForResource(mem_properties, mem_requirements,
                                 physical_device, device, memory))
    return false;
  vkBindBufferMemory(device, buffer, memory, 0);

  return true;
}

}  // namespace vk
}  // namespace utils