#include "model.h"

#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#include <iostream>

namespace utils {

namespace {

bool LoadObjFile(const std::string& path, Model* out_model,
                 std::string* out_mtl_path) {
  Model model;
  std::string mtl_path;

  std::ifstream file_strm(path);
  if (!file_strm.is_open())
    return false;

  std::string line;
  std::string token;

  std::vector<int> face_verts;

  while (std::getline(file_strm, line)) {
    std::stringstream sstrm(line);
    while (sstrm >> token) {
      if (token[0] == '#')
        break;

      if (token == "mtllib")  {
        sstrm >> mtl_path;
        if (mtl_path.empty())
          return false;

      } else if (token == "v") {
        float f1, f2, f3;
        sstrm >> f1 >> f2 >> f3;
        model.positions.push_back({ f1, f2, f3 });

      } else if (token == "f") {
        face_verts.clear();
        int i;
        while (sstrm >> i)
          face_verts.push_back(i);

        int top_idx = model.positions.size();
        for (int i = 0; i < face_verts.size(); ++i) {
          if (face_verts[i] < 0)
            face_verts[i] = top_idx + face_verts[i];

          if (face_verts[i] < 0)
            return false;
        }

        if (face_verts.size() == 3) {
          model.index_buffer.push_back(face_verts[0]);
          model.index_buffer.push_back(face_verts[1]);
          model.index_buffer.push_back(face_verts[2]);
        } else if (face_verts.size() == 4) {
          model.index_buffer.push_back(face_verts[0]);
          model.index_buffer.push_back(face_verts[1]);
          model.index_buffer.push_back(face_verts[2]);
          model.index_buffer.push_back(face_verts[0]);
          model.index_buffer.push_back(face_verts[2]);
          model.index_buffer.push_back(face_verts[3]);
        } else {
          return false;
        }
      }
    }
  }

  model.indexed = true;
  model.num_vertices = model.index_buffer.size();

  *out_mtl_path = std::move(mtl_path);
  *out_model = std::move(model);

  return true;
}

}  // namespace

bool LoadModel(const std::string& obj_path, const std::string& material_dir,
               Model* model) {
  std::string mtl_path;
  if (!LoadObjFile(obj_path, model, &mtl_path))
    return false;

  return true;
}

}  // namespace utils