#version 410

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 texcoord;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec3 binormal;

layout(location = 7) in vec4 instanceMatrix1;
layout(location = 8) in vec4 instanceMatrix2;
layout(location = 9) in vec4 instanceMatrix3;
layout(location = 10) in vec4 instanceMatrix4;

uniform int lables;
uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_view;
uniform mat4 vertex_view_to_projection;

out VS_OUT {
  vec3 normal;
  vec2 texcoord;
  vec3 tangent;
  vec3 binormal;
  mat3 TBN; // 输出 TBN
  mat4 normal_model_to_world;
}
vs_out;

void main() {
  vs_out.texcoord = texcoord.xy;

  mat4 model_to_world;
  vs_out.normal_model_to_world = transpose(inverse(model_to_world));

  if (lables == 0) {
    model_to_world = vertex_model_to_world;
    vs_out.normal_model_to_world = transpose(inverse(vertex_model_to_world));
  } else {
    mat4 instanceMatrix = mat4(instanceMatrix1, instanceMatrix2,
                               instanceMatrix3, instanceMatrix4);
    model_to_world = vertex_model_to_world * instanceMatrix;
    vs_out.normal_model_to_world = transpose(inverse(model_to_world));
  }
  vs_out.normal = vec3(vs_out.normal_model_to_world * vec4(normal, 0.0));
  // --- 计算 TBN  ---
  vec3 T = normalize(mat3(vs_out.normal_model_to_world) * tangent);
  vec3 N = normalize(mat3(vs_out.normal_model_to_world) * normal);
  T = normalize(T - dot(T, N) * N);
  vec3 B = cross(N, T);
  vs_out.TBN = mat3(T, B, N);

  gl_Position = vertex_view_to_projection * vertex_world_to_view *
                model_to_world * vec4(vertex, 1.0);
}
