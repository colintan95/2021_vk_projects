#ifndef UTILS_VK_H_
#define UTILS_VK_H_

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace utils {
namespace vk {

bool SupportsValidationLayers(const std::vector<const char*>& layers);

bool SupportsDeviceExtensions(VkPhysicalDevice physical_device,
                              const std::vector<const char*>& extensions);

bool CreateShaderModulesFromFiles(const std::vector<std::string>& file_paths,
                                  VkDevice device,
                                  std::vector<VkShaderModule>* shader_modules);

}  // namespace vk
}  // namespace utils

#endif  // UTILS_VK_H_