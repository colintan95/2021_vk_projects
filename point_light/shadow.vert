#version 450

layout(location = 0) in vec3 vert_pos;

layout(binding = 0) uniform UniformBufferObject {
  mat4 shadow_mat;
} ubo;

void main() {
  gl_Position = ubo.shadow_mat * vec4(vert_pos, 1.0);
}