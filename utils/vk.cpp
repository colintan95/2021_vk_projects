#include "utils/vk.h"

#include <vulkan/vulkan.h>

#include <fstream>
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

}  // namespace vk
}  // namespace utils