#version 450

layout(location = 0) flat in uint frag_material_index;

layout(location = 0) out vec4 out_color;

struct Material {
  vec3 ambient_color;
  vec3 diffuse_color;
};

layout(binding = 1) uniform UniformBufferObject {
  Material materials[20];
} ubo;

void main() {
  out_color = vec4(1.0, 0.0, 0.0, 1.0);
}