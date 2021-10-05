#version 450

layout(location = 0) in vec3 vert_pos;
layout(location = 1) in vec3 vert_normal;
layout(location = 2) in uint vert_mtl_idx;

layout(location = 0) out vec3 frag_world_pos;
layout(location = 1) out vec3 frag_normal;
layout(location = 2) flat out uint frag_mtl_idx;

layout(binding = 0) uniform UniformBufferObject {
  mat4 model_mat;
  mat4 mvp_mat;
} ubo;

void main() {
  frag_world_pos = (ubo.model_mat * vec4(vert_pos, 1.0)).xyz;
  frag_normal = vert_normal;
  frag_mtl_idx = vert_mtl_idx;

  gl_Position = ubo.mvp_mat * vec4(vert_pos, 1.0);
}