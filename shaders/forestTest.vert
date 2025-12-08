#version 410 core

layout(location = 0) in vec3 in_position;
uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_view;
uniform mat4 vertex_view_to_projection;

void main() {

  gl_Position = vertex_view_to_projection * vertex_world_to_view *
                vertex_model_to_world * vec4(in_position, 1.0);
}
