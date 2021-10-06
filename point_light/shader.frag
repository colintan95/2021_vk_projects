#version 450

layout(location = 0) in vec3 frag_world_pos;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) flat in uint frag_mtl_idx;

layout(location = 0) out vec4 out_color;

struct Material {
  vec4 ambient_color;
  vec4 diffuse_color;
};

layout(binding = 1) uniform UniformBufferObject {
  vec4 light_pos;
  Material materials[20];
} ubo;

layout(binding = 2) uniform samplerCubeShadow shadow_tex_sampler;

void main() {
  vec3 l = normalize(ubo.light_pos.xyz - frag_world_pos);
  vec3 n = normalize(frag_normal);

  vec3 ambient = ubo.materials[frag_mtl_idx].ambient_color.rgb;
  ambient *= 0.3;

  vec3 diffuse = ubo.materials[frag_mtl_idx].diffuse_color.rgb;
  diffuse *= clamp(dot(l, n), 0, 1);

  float depth = texture(shadow_tex_sampler, vec4(-l, 0.0));

  out_color = vec4(ambient + diffuse, 1.0);
}