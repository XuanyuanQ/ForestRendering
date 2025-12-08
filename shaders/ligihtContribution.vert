#version 410

layout(location = 0) in vec3 vertex;

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_view;
uniform mat4 vertex_view_to_projection;
uniform mat4 vertex_world_to_clip;

void main() {
  mat4 model_to_world = vertex_model_to_world;

  gl_Position = vec4(vertex, 1.0);
}

// layout(location = 0) in vec3 vertex;
// layout(location = 2) in vec2 texcoord;

// uniform mat4 vertex_model_to_world;
// uniform mat4 vertex_world_to_clip;

// void main() {
//   TexCoords = texcoord;
//   gl_Position =
//       vertex_world_to_clip * vertex_model_to_world * vec4(vertex, 1.0);
// }
