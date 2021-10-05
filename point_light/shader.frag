#version 450

layout(location = 0) flat in uint frag_material_index;

layout(location = 0) out vec4 out_color;

struct Material {
  vec4 ambient_color;
  vec4 diffuse_color;
};

layout(binding = 1) uniform UniformBufferObject {
  vec4 light_position;
  Material materials[20];
} ubo;

void main() {
  out_color = vec4(ubo.materials[frag_material_index].ambient_color.rgb, 1.0);
}