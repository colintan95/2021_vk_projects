#include "model.h"

#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include <iostream>

namespace utils {

namespace {

bool LoadMaterialFile(const std::string& path, std::vector<Material>* materials,
                      std::unordered_map<std::string, int>* mtl_name_to_idx) {
  materials->clear();
  mtl_name_to_idx->clear();

  std::ifstream file_strm(path);
  if (!file_strm.is_open())
    return false;

  std::string line;
  std::string token;

  Material mtl = {};
  std::string mtl_name;
  bool mtl_pending = false;

  while(std::getline(file_strm, line)) {
    std::stringstream sstrm(line);
    while (sstrm >> token) {
      if (token == "newmtl") {
        if (mtl_pending) {
          materials->push_back(mtl);
          (*mtl_name_to_idx)[mtl_name] = materials->size() - 1;
          mtl_pending = false;
        }
        mtl = {};
        mtl_name = "";
        sstrm >> mtl_name;
        if (mtl_name.empty())
          return false;
        mtl_pending = true;

      } else if (token == "Ka") {
        float f1, f2, f3;
        sstrm >> f1 >> f2 >> f3;
        mtl.ambient_color = { f1, f2, f3 };

      } else if (token == "Kd") {
        float f1, f2, f3;
        sstrm >> f1 >> f2 >> f3;
        mtl.diffuse_color = { f1, f2, f3 };
      }
    }
  }

  if (mtl_pending) {
    materials->push_back(mtl);
    (*mtl_name_to_idx)[mtl_name] = materials->size() - 1;
    mtl_pending = false;
  }

  return true;
}

}  // namespace

bool LoadModel(const std::string& obj_path, Model* out_model) {
  Model model;
  std::string mtl_path;

  std::ifstream file_strm(obj_path);
  if (!file_strm.is_open())
    return false;

  std::string line;
  std::string token;

  std::vector<Material> materials;
  std::unordered_map<std::string, int> mtl_name_to_idx;

  std::vector<glm::vec3> positions;
  std::vector<int> face_pos_indices;
  std::vector<int> face_normal_indices;

  uint16_t current_idx = 0;
  std::string mtl_name;
  int mtl_idx;

  while (std::getline(file_strm, line)) {
    std::stringstream sstrm(line);
    while (sstrm >> token) {
      if (token[0] == '#')
        break;

      if (token == "mtllib")  {
        sstrm >> mtl_path;
        if (mtl_path.empty())
          return false;

        if (!LoadMaterialFile(mtl_path, &materials, &mtl_name_to_idx))
          return false;

      } else if (token == "usemtl") {
        mtl_name = "";
        sstrm >> mtl_name;
        if (mtl_name.empty())
          return false;

        auto it = mtl_name_to_idx.find(mtl_name);
        if (it == mtl_name_to_idx.end())
          return false;
        mtl_idx = it->second;

      } else if (token == "v") {
        float f1, f2, f3;
        sstrm >> f1 >> f2 >> f3;
        positions.push_back({ f1, f2, f3 });

      } else if (token == "f") {
        face_pos_indices.clear();
        face_normal_indices.clear();

        int i;
        while (sstrm >> i)
          face_pos_indices.push_back(i);

        int top_pos_idx = positions.size();
        for (int i = 0; i < face_pos_indices.size(); ++i) {
          if (face_pos_indices[i] < 0)
            face_pos_indices[i] = top_pos_idx + face_pos_indices[i];

          if (face_pos_indices[i] < 0)
            return false;
        }

        if (face_pos_indices.size() == 3) {
          model.positions.push_back(positions[face_pos_indices[0]]);
          model.positions.push_back(positions[face_pos_indices[1]]);
          model.positions.push_back(positions[face_pos_indices[2]]);

          if (face_normal_indices.empty()) {
            glm::vec3 n1 = glm::cross(positions[face_pos_indices[1]] -
                                          positions[face_pos_indices[0]],
                                      positions[face_pos_indices[2]] -
                                          positions[face_pos_indices[0]]);
            n1 = glm::normalize(n1);
            model.normals.push_back(n1);
            model.normals.push_back(n1);
            model.normals.push_back(n1);
          }

          model.material_indices.push_back(mtl_idx);
          model.material_indices.push_back(mtl_idx);
          model.material_indices.push_back(mtl_idx);

          model.index_buffer.push_back(current_idx);
          model.index_buffer.push_back(current_idx + 1);
          model.index_buffer.push_back(current_idx + 2);

          current_idx += 3;
        } else if (face_pos_indices.size() == 4) {
          model.positions.push_back(positions[face_pos_indices[0]]);
          model.positions.push_back(positions[face_pos_indices[1]]);
          model.positions.push_back(positions[face_pos_indices[2]]);
          model.positions.push_back(positions[face_pos_indices[0]]);
          model.positions.push_back(positions[face_pos_indices[2]]);
          model.positions.push_back(positions[face_pos_indices[3]]);

          if (face_normal_indices.empty()) {
            glm::vec3 n1 = glm::cross(positions[face_pos_indices[1]] -
                                          positions[face_pos_indices[0]],
                                      positions[face_pos_indices[2]] -
                                          positions[face_pos_indices[0]]);
            glm::vec3 n2 = glm::cross(positions[face_pos_indices[2]] -
                                          positions[face_pos_indices[0]],
                                      positions[face_pos_indices[3]] -
                                          positions[face_pos_indices[0]]);
            n1 = glm::normalize(n1);
            n2 = glm::normalize(n2);
            model.normals.push_back(n1);
            model.normals.push_back(n1);
            model.normals.push_back(n1);
            model.normals.push_back(n2);
            model.normals.push_back(n2);
            model.normals.push_back(n2);
          }

          model.material_indices.push_back(mtl_idx);
          model.material_indices.push_back(mtl_idx);
          model.material_indices.push_back(mtl_idx);
          model.material_indices.push_back(mtl_idx);
          model.material_indices.push_back(mtl_idx);
          model.material_indices.push_back(mtl_idx);

          model.index_buffer.push_back(current_idx);
          model.index_buffer.push_back(current_idx + 1);
          model.index_buffer.push_back(current_idx + 2);
          model.index_buffer.push_back(current_idx + 3);
          model.index_buffer.push_back(current_idx + 4);
          model.index_buffer.push_back(current_idx + 5);

          current_idx += 6;
        } else {
          return false;
        }
      }
    }
  }

  model.materials = std::move(materials);

  *out_model = std::move(model);

  return true;
}

}  // namespace utils