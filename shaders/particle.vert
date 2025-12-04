#version 410

layout(location = 0) in vec4 vertex;
// layout(location = 2) in vec3 texcoord;

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_clip;

uniform vec2 offset;
uniform vec4 color;

out vec2 TexCoords;
out vec4 ParticleColor;

// out VS_OUT { vec2 texcoord; }
// vs_out;

void main() {
  float scale = 0.4f;
  TexCoords = vertex.zw;
  ParticleColor = color;
  gl_Position = vertex_world_to_clip * vertex_model_to_world *
                vec4((vertex.xy * scale) + offset, 0.0, 1.0);
  // gl_Position =
  //     vertex_world_to_clip * vertex_model_to_world * vec4(vertex.xy,
  //     0.0, 1.0);
}
