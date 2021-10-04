#ifndef UTILS_MODEL_H_
#define UTILS_MODEL_H_

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace utils {

struct Model {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<uint16_t> index_buffer;

  int num_vertices = -1;
  bool indexed = false;
};

bool LoadModel(const std::string& obj_path, const std::string& material_dir,
               Model* model);

}  // namespace utils

#endif  // UTILS_MODEL_H_