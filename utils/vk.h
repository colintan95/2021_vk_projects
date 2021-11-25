#ifndef UTILS_VK_H_
#define UTILS_VK_H_

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace utils {
namespace vk {

bool CreateShaderModulesFromFiles(const std::vector<std::string>& file_paths,
                                  VkDevice device,
                                  std::vector<VkShaderModule>* shader_modules);

}  // namespace vk
}  // namespace utils

#endif  // UTILS_VK_H_