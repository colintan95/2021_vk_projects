#ifndef UTILS_MODEL_H_
#define UTILS_MODEL_H_

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace utils {

struct Material {
  glm::vec3 ambient_color;
  glm::vec3 diffuse_color;
};

struct Model {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<uint16_t> index_buffer;

  std::vector<uint32_t> material_indices;

  std::vector<Material> materials;
};

bool LoadModel(const std::string& obj_path, Model* out_model);

}  // namespace utils

#endif  // UTILS_MODEL_H_