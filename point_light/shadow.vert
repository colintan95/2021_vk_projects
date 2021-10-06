#version 450

layout(location = 0) in vec3 vert_pos;

layout(push_constant) uniform ConstantBlock {
  mat4 shadow_mat;
} cb;

void main() {
  gl_Position = cb.shadow_mat * vec4(vert_pos, 1.0);
}