#version 410
layout(location = 0) in vec3 vertex;
// layout(location = 1) in vec3 normal;
layout(location = 1) in vec2 texcoord;
// layout(location = 3) in vec3 tangent;
// layout(location = 4) in vec3 binormal;

// out VS_OUT { vec2 TextureCoord; }
// vs_out;
out vec2 vs_texcoord;

// uniform vec3 light_position;
// uniform vec3 camera_position;

void main() {

  vs_texcoord = texcoord.xy;
  gl_Position = vec4(vertex, 1.0);
}