#include "utils/vk.h"

#include <fstream>

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