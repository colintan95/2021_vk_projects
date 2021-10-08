#include "utils/camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

namespace utils {

void Camera::StartMovement(Direction direction, float speed) {
  speeds_[direction] = speed;
}

void Camera::StopMovement(Direction direction) {
  auto it = speeds_.find(direction);
  if (it != speeds_.end())
    speeds_.erase(it);
}

void Camera::MoveByIncrement(Direction direction, float increment) {
  switch (direction) {
    case Direction::kPosX:
      position_ += glm::vec3(increment, 0.f, 0.f);
      break;
    case Direction::kNegX:
      position_ += glm::vec3(-increment, 0.f, 0.f);
      break;
    case Direction::kPosY:
      position_ += glm::vec3(0.f, increment, 0.f);
      break;
    case Direction::kNegY:
      position_ += glm::vec3(0.f, -increment, 0.f);
      break;
    case Direction::kPosZ:
      position_ += glm::vec3(0.f, 0.f, increment);
      break;
    case Direction::kNegZ:
      position_ += glm::vec3(0.f, 0.f, -increment);
      break;
    case Direction::kPosPitch:
      pitch_ += increment;
      break;
    case Direction::kNegPitch:
      pitch_ -= increment;
      break;
    case Direction::kPosYaw:
      yaw_ += increment;
      break;
    case Direction::kNegYaw:
      yaw_ -= increment;
      break;
  }
}

void Camera::SetPosition(glm::vec3 position) {
  position_ = position;
}

glm::mat4 Camera::GetViewMat() {
  return glm::rotate(glm::mat4(1.f), pitch_, glm::vec3(0.f, 0.f, 1.f)) *
      glm::rotate(glm::mat4(1.f), yaw_, glm::vec3(0.f, 1.f, 0.f)) *
      glm::translate(glm::mat4(1.f), -position_);
}

void Camera::Tick(float time_delta) {
  for (auto [direction, speed] : speeds_) {
    if (speed != 0.f)
      MoveByIncrement(direction, speed * (time_delta / 1000.f));
  }
}

}  // namespace utils