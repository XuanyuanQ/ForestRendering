#version 410 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 texcoord;

layout(location = 7) in vec4 instanceMatrix1;
layout(location = 8) in vec4 instanceMatrix2;
layout(location = 9) in vec4 instanceMatrix3;
layout(location = 10) in vec4 instanceMatrix4;
uniform vec3 camera_position;

// 光源参数
uniform vec3 light_position;
uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_view;
uniform mat4 vertex_view_to_projection;

uniform float u_Time;           // 全局时间
uniform vec3 u_TreeCrownCenter; // 树冠中心点 (粒子发射源)
uniform vec3 u_TreeCrownSize;   // 树冠大小范围 (例如 x=5, y=3, z=5)

out VS_OUT {
  vec3 normal;
  vec2 texcoord;
  vec3 fV;
  vec3 fL;
}
vs_out;
flat out int v_InstanceID;
flat out int v_VertexID;
float random(vec2 st) {
  return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

float waveFun(float time, float A, float f, float p, float k, vec2 D,
              vec3 point) {
  float a = sin((D.x * point.x + (D.y) * point.z) * f + time * p) * 0.5 + 0.5;
  return A * pow(a, k);
}

float derivativeMain(float time, float A, float f, float p, float k, vec2 D,
                     vec3 point) {
  float wave = waveFun(time, A, f, p, max(0, k - 1.0), D, point);
  return 0.5 * k * f * wave *
         cos((D.x * point.x + (D.y) * point.z) * f + time * p);
}

void main() {
  v_InstanceID = gl_InstanceID;
  v_VertexID = gl_VertexID;
  vs_out.texcoord = texcoord.xy;

  mat4 instanceMatrix =
      mat4(instanceMatrix1, instanceMatrix2, instanceMatrix3, instanceMatrix4);
  vec4 model_pos =
      vertex_model_to_world * instanceMatrix * vec4(in_position * 0.05, 1.0);
  vs_out.normal = (transpose(inverse(vertex_model_to_world * instanceMatrix)) *
                   vec4(normal, 0.0))
                      .rgb;
  float wave1 =
      waveFun(1.0, 1.0, 0.2, 0.5, 2.0, vec2(-1.0, 0.0), model_pos.xyz);
  float wave2 =
      waveFun(1.0, 0.5, 0.4, 1.3, 2.0, vec2(-0.7, 0.7), model_pos.xyz);
  float heightOffset = 1.0 * (wave1 + wave2);
  model_pos.y += heightOffset + 4.0;
  float id = float(gl_InstanceID);

  // --- 1. 生命周期的管理 (关键) ---
  // 我们希望叶子源源不断地产生。
  // 假设每片叶子的寿命是 10秒。
  float leafLifeSpan = 50.0;
  // 利用时间偏移，让不同 ID 的叶子在不同时间出生
  float birthTime = id * 0.5; // 每隔 0.1秒出生一片
  float localizedTime = u_Time - birthTime;

  // 计算当前叶子处于生命周期的哪个阶段 (0.0 = 刚出生, 1.0 = 该消失了)
  // 使用 mod 实现循环播放
  float lifePhase = mod(localizedTime, leafLifeSpan) / leafLifeSpan;

  // --- 2. 确定出生位置 (随机在树冠内) ---
  vec3 spawnOffset;
  spawnOffset.x = (random(vec2(id, 1.0)) - 0.5) * u_TreeCrownSize.x;
  spawnOffset.y = (random(vec2(id, 2.0)) - 0.5) * u_TreeCrownSize.y;
  spawnOffset.z = (random(vec2(id, 3.0)) - 0.5) * u_TreeCrownSize.z;
  vec3 startPosition = u_TreeCrownCenter + spawnOffset;
  vec3 finalVertexPos = vec3(100.0);
  // --- 3. 计算下落动画 ---
  vec3 currentPos = startPosition;
  // Y轴：匀速下落 (也可以加上重力加速)
  float dropSpeed =
      pow(0.8 * random(vec2(id, 4.0)), 2.0) * u_Time + 1.0; // 随机速度
  currentPos.y -= lifePhase * leafLifeSpan * dropSpeed;
  float new_height = model_pos.y + currentPos.y;
  vec3 CalPos = model_pos.xyz + currentPos;
  float boundary = 40;
  if (new_height > model_pos.y) {
    // XZ轴：随风飘荡 (正弦波)
    float wobbleFreq = 0.4;
    float wobbleAmp = 10.5;
    currentPos.x += sin(u_Time * wobbleFreq + id) * wobbleAmp;
    currentPos.z += cos(u_Time * wobbleFreq + id * 1.5) * wobbleAmp;
  } else {
    new_height = model_pos.y;
  }
  // --- 4. (可选) 落地后隐藏 ---
  // 如果掉到地面以下，可以把它缩放到 0 看不见，等待下一次循环重生
  float scale = 1.0;

  if (CalPos.z > boundary || CalPos.z < -boundary) {
    // finalVertexPos = model_pos.xyz;
    scale = 0.0;
  } else if (CalPos.x > boundary || CalPos.x < -boundary) {
    // finalVertexPos = model_pos.xyz;
    scale = 0.0;
  }
  // finalVertexPos = model_pos.xyz;
  finalVertexPos = model_pos.xyz + currentPos;
  vs_out.fV =
      camera_position - vec3(finalVertexPos.x, new_height, finalVertexPos.z);
  // add 模拟太阳光：光照方向 = 光源本身的方向向量
  vs_out.fL = light_position;
  gl_Position =
      vertex_view_to_projection * vertex_world_to_view *
      vec4(vec3(finalVertexPos.x, new_height, finalVertexPos.z) * scale, 1.0);
  if (scale == 0.0) {
    gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
  }
}
