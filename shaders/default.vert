#version 410

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 texcoord;

layout(location = 3) in vec4 instanceMatrix1;
layout(location = 4) in vec4 instanceMatrix2;
layout(location = 5) in vec4 instanceMatrix3;
layout(location = 6) in vec4 instanceMatrix4;

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_clip;
// uniform mat4 normal_model_to_world;
uniform vec3 light_position;
uniform vec3 camera_position;

out VS_OUT {
  vec3 normal;
  vec2 texcoord;
  vec3 fV;
  vec3 fL;
}
vs_out;

void main() {
  mat4 instanceMatrix =
      mat4(instanceMatrix1, instanceMatrix2, instanceMatrix3, instanceMatrix4);
  mat4 model_to_world = vertex_model_to_world * instanceMatrix;
  mat4 normal_model_to_world = transpose(inverse(model_to_world));

  vs_out.texcoord = texcoord.xy;
  vs_out.normal = vec3(normal_model_to_world * vec4(normal, 0.0));
  vs_out.fV = camera_position - vec3(model_to_world * vec4(vertex, 1.0));
  vs_out.fL = light_position - vec3(model_to_world * vec4(vertex, 1.0));
  gl_Position = vertex_world_to_clip * model_to_world * vec4(vertex, 1.0);
}
