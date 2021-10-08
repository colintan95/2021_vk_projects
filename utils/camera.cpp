#include "utils/camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

namespace utils {

void Camera::SetPosition(glm::vec3 position) {
  position_ = position;
}

glm::mat4 Camera::GetViewMat() {
  return glm::rotate(glm::mat4(1.f), pitch_, glm::vec3(0.f, 0.f, 1.f)) *
      glm::rotate(glm::mat4(1.f), yaw_, glm::vec3(0.f, 1.f, 0.f)) *
      glm::translate(glm::mat4(1.f), -position_);
}

}  // namespace utils