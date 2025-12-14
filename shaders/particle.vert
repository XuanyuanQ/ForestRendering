#version 410 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 texcoord;
layout(location = 7) in vec4 instanceMatrix1;
layout(location = 8) in vec4 instanceMatrix2;
layout(location = 9) in vec4 instanceMatrix3;
layout(location = 10) in vec4 instanceMatrix4;

uniform vec3 camera_position;
uniform vec3 light_position;
uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_view;
uniform mat4 vertex_view_to_projection;

uniform float u_Time;
uniform vec3 u_TreeCrownCenter;
uniform vec3 u_TreeCrownSize;

out VS_OUT {
  vec3 normal;
  vec2 texcoord;
  vec3 fV;
  vec3 fL;
} vs_out;

flat out int v_InstanceID;
flat out int v_VertexID;

float random(vec2 st) {
  return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

float waveFun(float time, float A, float f, float p, float k, vec2 D, vec3 point) {
  float a = sin((D.x * point.x + (D.y) * point.z) * f + time * p) * 0.5 + 0.5;
  return A * pow(a, k);
}

void main() {
  v_InstanceID = gl_InstanceID;
  v_VertexID = gl_VertexID;
  vs_out.texcoord = texcoord.xy;

  mat4 instanceMatrix = mat4(instanceMatrix1, instanceMatrix2, instanceMatrix3, instanceMatrix4);
  

  vec4 model_pos = vertex_model_to_world * instanceMatrix * vec4(in_position, 1.0);

  vs_out.normal = (transpose(inverse(vertex_model_to_world * instanceMatrix)) * vec4(normal, 0.0)).rgb;

  // --- 地形起伏逻辑 ---
  float wave1 = waveFun(1.0, 1.0, 0.2, 0.5, 2.0, vec2(-1.0, 0.0), model_pos.xyz);
  float wave2 = waveFun(1.0, 0.5, 0.4, 1.3, 2.0, vec2(-0.7, 0.7), model_pos.xyz);
  float heightOffset = 1.0 * (wave1 + wave2);
  
  model_pos.y += heightOffset + 4.0;

  float id = float(gl_InstanceID);

  // 1. 生命周期
  float leafLifeSpan = 50.0;
  float birthTime = id * 0.5;
  float localizedTime = u_Time - birthTime;
  float lifePhase = mod(localizedTime, leafLifeSpan) / leafLifeSpan;

  // 2. 确定出生位置
  vec3 startPosition = vec3(0.0);

  // 计算下落动画
  vec3 currentPos = startPosition;
  
  // 下落速度逻辑
  float dropSpeed = pow(0.8 * random(vec2(id, 4.0)), 2.0) * u_Time + 1.0;
  
  // 应用下落 (只减 Y 轴)
  float dropDistance = mod(u_Time * dropSpeed + id * 5.0, 15.0); // 循环掉落 15米
  currentPos.y -= dropDistance;

  float new_height = model_pos.y + currentPos.y;
  vec3 CalPos = model_pos.xyz + currentPos;

  // 扩大边界检查 (Boundary)
  float boundary = 200.0;

  // 摇摆逻辑
  if (new_height > model_pos.y - 20.0) { // 只要没掉得太深
	float wobbleFreq = 0.4;
	float wobbleAmp = 10.5;
	currentPos.x += sin(u_Time * wobbleFreq + id) * wobbleAmp;
	currentPos.z += cos(u_Time * wobbleFreq + id * 1.5) * wobbleAmp;
  }

  // 缩放逻辑 (落地隐藏)
  float scale = 1.0;
  if (CalPos.z > boundary || CalPos.z < -boundary) {
	scale = 0.0;
  } else if (CalPos.x > boundary || CalPos.x < -boundary) {
	scale = 0.0;
  }

  vec3 finalVertexPos = model_pos.xyz + currentPos;

  // 输出
  vs_out.fV = camera_position - vec3(finalVertexPos.x, new_height, finalVertexPos.z);
  vs_out.fL = light_position;
  
  gl_Position = vertex_view_to_projection * vertex_world_to_view *
	  vec4(vec3(finalVertexPos.x, new_height, finalVertexPos.z) * scale, 1.0);

  if (scale == 0.0) {
	gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
  }
}
