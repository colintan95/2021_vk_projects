#ifndef UTILS_CAMERA_H_
#define UTILS_CAMERA_H_

#include <glm/glm.hpp>

namespace utils {

class Camera {
public:
  void SetPosition(glm::vec3 position);

  glm::mat4 GetViewMat();

private:
  glm::vec3 position_;
  float pitch_ = 0.f;  // in radians.
  float yaw_ = 0.f;  // in radians.
};

}  // namespace utils

#endif  // UTILS_CAMERA_H_