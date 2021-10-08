#ifndef UTILS_CAMERA_H_
#define UTILS_CAMERA_H_

#include <glm/glm.hpp>

#include <unordered_map>

namespace utils {

class Camera {
public:
  enum class Direction {
    kPosX, kNegX,
    kPosY, kNegY,
    kPosZ, kNegZ,
    kPosPitch, kNegPitch,
    kPosYaw, kNegYaw
  };

  // |speed| should be per second.
  void StartMovement(Direction direction, float speed);
  void StopMovement(Direction direction);

  void MoveByIncrement(Direction direction, float increment);

  void SetPosition(glm::vec3 position);

  glm::mat4 GetViewMat();

  // |time_delta| should be in milliseconds.
  void Tick(float time_delta);

private:
  // Speeds are per second.
  std::unordered_map<Direction, float> speeds_;

  glm::vec3 position_;
  float pitch_ = 0.f;  // in radians.
  float yaw_ = 0.f;  // in radians.
};

}  // namespace utils

#endif  // UTILS_CAMERA_H_