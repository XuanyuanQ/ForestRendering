#version 410

layout(location = 0) in vec3 vertex;

uniform mat4 vertex_world_to_clip;

out vec3 localPos;

void main() {
  localPos = vertex;

  gl_Position = vertex_world_to_clip * vec4(vertex, 1.0);
}
