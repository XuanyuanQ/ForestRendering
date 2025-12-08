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
uniform mat4 light_world_to_clip_matrix;
// 摄像机位置
uniform vec3 camera_position;

// 光源参数
uniform vec3 light_position;
uniform vec3 light_direction;

out VS_OUT {
  vec3 normal;
  vec2 texcoord;
  vec3 tangent;
  vec3 binormal;
  mat3 TBN; // 输出 TBN
  vec4 ndc;
  vec3 world_pos;
  mat4 normal_model_to_world;
  vec3 fV;
  vec3 fL;
  vec4 FragPosLightSpace;
}
vs_out;

void main() {
  vs_out.texcoord = texcoord.xy;

  mat4 model_to_world;
  // vs_out.normal_model_to_world = transpose(inverse(model_to_world));

  if (lables == 0) {
    model_to_world = vertex_model_to_world;
  } else {
    mat4 instanceMatrix = mat4(instanceMatrix1, instanceMatrix2,
                               instanceMatrix3, instanceMatrix4);
    model_to_world = vertex_model_to_world * instanceMatrix;
  }
  vs_out.normal_model_to_world = transpose(inverse(model_to_world));
  float scale = 1.0;
  if (lables == 3) {
    scale = 0.02;
  }
  vs_out.normal = vec3(vs_out.normal_model_to_world * vec4(normal, 0.0));
  // --- 计算 TBN  ---
  vec3 T = normalize(mat3(vs_out.normal_model_to_world) * tangent);
  vec3 N = normalize(mat3(vs_out.normal_model_to_world) * normal);
  T = normalize(T - dot(T, N) * N);
  vec3 B = cross(N, T);
  vs_out.TBN = mat3(T, B, N);
  vs_out.ndc = vertex_view_to_projection * vertex_world_to_view *
               model_to_world * vec4(vertex * scale, 1.0);
  vs_out.world_pos = vec3(model_to_world * vec4(vertex * scale, 1.0));
  vs_out.FragPosLightSpace =
      light_world_to_clip_matrix * (model_to_world * vec4(vertex * scale, 1.0));
  vs_out.fV =
      camera_position - vec3(model_to_world * vec4(vertex * scale, 1.0));
  // add 模拟太阳光：光照方向 = 光源本身的方向向量
  vs_out.fL = light_position;

  gl_Position = vertex_view_to_projection * vertex_world_to_view *
                model_to_world * vec4(vertex * scale, 1.0);
}
