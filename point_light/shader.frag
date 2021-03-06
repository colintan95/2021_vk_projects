#version 450

layout(location = 0) in vec3 frag_world_pos;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) flat in uint frag_mtl_idx;

layout(location = 0) out vec4 out_color;

struct Material {
  vec4 ambient_color;
  vec4 diffuse_color;
};

layout(binding = 1, std140) uniform UniformBufferObject {
  vec4 light_pos;
  float shadow_near_plane;
  float shadow_far_plane;
  vec2 pad;
  Material materials[20];
} ubo;

layout(binding = 2) uniform samplerCube shadow_tex_sampler;

void main() {
  vec3 light_vec = ubo.light_pos.xyz - frag_world_pos;

  vec3 l = normalize(light_vec);
  vec3 n = normalize(frag_normal);

  vec3 ambient = ubo.materials[frag_mtl_idx].ambient_color.rgb;
  ambient *= 0.3;

  vec3 diffuse = ubo.materials[frag_mtl_idx].diffuse_color.rgb;
  diffuse *= clamp(dot(l, n), 0, 1);

  float near = ubo.shadow_near_plane;
  float far = ubo.shadow_far_plane;

  float max_com = max(max(abs(light_vec.x), abs(light_vec.y)),
                      abs(light_vec.z));
  float depth = (-far + near * far / max_com) / (near - far);
  depth = clamp(depth, 0, 1);

  // Increase the bias as the cubemap coord gets closer to the edges of the
  // cube and as the depth increases.
  float depth_bias = (1 - depth) * (0.1 + (1 - max_com) * 0.1);
  depth = clamp(depth - depth_bias, 0, 1);

  vec3 cubemap_coord = -l;
  // Inverts the x coord because cubemaps use left-handed coordinates.
  cubemap_coord.x *= -1;

  float shadow_tex_depth = texture(shadow_tex_sampler, cubemap_coord).r;

  float no_shadow = clamp(sign(shadow_tex_depth - depth), 0, 1);

  out_color = vec4(ambient + no_shadow * diffuse, 1.0);
}