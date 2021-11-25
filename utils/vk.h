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

bool CreateImage(const VkImageCreateInfo& image_info,
                 VkMemoryPropertyFlags mem_properties,
                 VkPhysicalDevice physical_device, VkDevice device,
                 VkImage& image, VkDeviceMemory& memory);

bool CreateBuffer(const VkBufferCreateInfo& buffer_info,
                  VkMemoryPropertyFlags mem_properties,
                  VkPhysicalDevice physical_device, VkDevice device,
                  VkBuffer& buffer, VkDeviceMemory& memory);

}  // namespace vk
}  // namespace utils

#endif  // UTILS_VK_H_