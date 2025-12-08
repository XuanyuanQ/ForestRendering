#version 410

layout(location = 0) in vec3 vertex;
layout(location = 2) in vec3 texcoord;

layout(location = 7) in vec4 instanceMatrix1;
layout(location = 8) in vec4 instanceMatrix2;
layout(location = 9) in vec4 instanceMatrix3;
layout(location = 10) in vec4 instanceMatrix4;

uniform mat4 vertex_model_to_world;
uniform mat4 light_world_to_clip_matrix;
uniform int lables;
out VS_OUT { vec2 texcoord; }
vs_out;
void main() {
  vs_out.texcoord = texcoord.xy;

  mat4 instanceMatrix =
      mat4(instanceMatrix1, instanceMatrix2, instanceMatrix3, instanceMatrix4);
  mat4 model_to_world = vertex_model_to_world * instanceMatrix;
  if (lables == 0) {
    model_to_world = vertex_model_to_world;
  }
  float scale = 1.0;
  if (lables == 3) {
    scale = 0.02;
  }
  gl_Position =
      light_world_to_clip_matrix * model_to_world * vec4(scale * vertex, 1.0);
}
