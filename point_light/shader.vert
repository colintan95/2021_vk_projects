#version 450

layout(location = 0) in vec2 vert_position;

layout(binding = 0) uniform UniformBufferObject {
  mat4 mvp_mat;
} ubo;

void main() {
  gl_Position = ubo.mvp_mat * vec4(vert_position, 0.0, 1.0);
}