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
layout(location = 11) in float instanceWindsSpeed;

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

// 接收时间与风力
uniform float elapsed_time_s;
uniform float wind_strength;

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
flat out int v_InstanceID;
flat out int v_VertexID;
float random(float seed) { return fract(sin(seed) * 43758.5453123); }

// --- 新增：旋转矩阵辅助函数 ---
mat3 angleAxis(float angle, vec3 axis) {
	axis = normalize(axis);
	float s = sin(angle);
	float c = cos(angle);
	float oc = 1.0 - c;
	return mat3(
		oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
		oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
		oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c
	);
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


void applyWind(inout vec3 localPos, inout vec3 localNormal, inout vec3 localTangent,
			   int labelType, float windPower, float time, float seed) {
	
	if (windPower <= 0.0) return;

	switch (labelType) {
	case 1: // Leaves
	{
		// 1. 宏观摆动 (模拟树枝晃动)
		// 频率 1.5，使用 seed 让不同树错开
		float swayAngle = sin(time * 1.5 + seed) * 0.1 * windPower;
		vec3 swayAxis = vec3(0.0, 0.0, 1.0); // 绕 Z 轴晃
		mat3 rotSway = angleAxis(swayAngle, swayAxis);

		// 2. 微观颤动 (模拟叶子乱颤)
		// 频率 15.0，使用 localPos 作为空间噪声
		float flutterPhase = dot(localPos, vec3(10.0, 20.0, 30.0));
		float flutterAngle = sin(time * 15.0 + flutterPhase) * 0.2 * windPower;
		
		// 随机轴颤动
		vec3 flutterAxis = normalize(vec3(random(seed), random(seed + 123.0), random(seed + 456.0)));
		mat3 rotFlutter = angleAxis(flutterAngle, flutterAxis);

		// 3. 组合并应用
		mat3 totalRot = rotSway * rotFlutter;

		// 旋转局部坐标和法线/切线
		localPos     = totalRot * localPos;
		localNormal  = totalRot * localNormal;
		localTangent = totalRot * localTangent;
		break;
	}

	case 3: // Grass
	{
		float wave = sin(time * 3.0 + seed + localPos.x * 2.0);
		float bend = pow(max(0.0, localPos.y), 1.5);
		
		float displacement = wave * bend * 0.5 * windPower;
		localPos.x += displacement;
		localPos.z += displacement * 0.5;
		break;
	}

	default:
		break;
	}
}

void main() {
  v_InstanceID = gl_InstanceID;
  v_VertexID = gl_VertexID;
  vs_out.texcoord = texcoord.xy;
  mat4 model_to_world;
  // vs_out.normal_model_to_world = transpose(inverse(model_to_world));
  vec3 world_pos;
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
	scale = 0.008;
  }
  // if (lables == 0) {
  //   scale = 3.0;
  // }
  // if ( == 1 || lables == 2) {
  //   scale = 0.8;
  // }lables


  vec3 localPos = vertex * scale;
  vec3 localNormal = normal;
  vec3 localTangent = tangent;


  float instanceSpeed = instanceWindsSpeed > 0.0 ? instanceWindsSpeed : 1.0;
  float finalWind = wind_strength * instanceSpeed;

  applyWind(localPos, localNormal, localTangent,
			lables, finalWind, elapsed_time_s, float(v_InstanceID));

  world_pos = vec3(model_to_world * vec4(localPos, 1.0));

  vs_out.normal = vec3(vs_out.normal_model_to_world * vec4(normal, 0.0));

  // --- 计算 TBN  ---
  // 使用旋转后的 localNormal/Tangent，确保光照跟随摆动
  // 注意：需要变换到世界空间
  mat3 normalMatrix = mat3(vs_out.normal_model_to_world);
  vec3 N = normalize(normalMatrix * localNormal);
  vec3 T = normalize(normalMatrix * localTangent);

  T = normalize(T - dot(T, N) * N);
  vec3 B = cross(N, T);
  
  // 更新 vs_out 的 normal 和 TBN
  vs_out.normal = N;
  vs_out.TBN = mat3(T, B, N);

  float time = 1.0;
  // 计算两个波形叠加
  float wave1 = waveFun(time, 1.0, 0.2, 0.5, 2.0, vec2(-1.0, 0.0), vertex);
  float wave2 = waveFun(time, 0.5, 0.4, 1.3, 2.0, vec2(-0.7, 0.7), vertex);

  // 应用高度偏移
  // 注意：这里修改的是
  // worldPos.y，这样不仅位置变了，后续的光照计算也会基于这个新高度
  float heightOffset = 1.0 * (wave1 + wave2);
  
  // 原有的 world_pos 计算被上面“改动3”替换了，但保留高度偏移逻辑
  world_pos.y += heightOffset;

  // 原有的风吹逻辑块被 applyWind 函数调用替换

  if (lables == 3) {
	world_pos.y = -world_pos.y;
  }
  if (lables == 1 || lables == 2) {
	world_pos.y += 3.0;
  }
  vs_out.ndc = vertex_view_to_projection * vertex_world_to_view *
			   model_to_world * vec4(world_pos, 1.0);


  vs_out.world_pos = world_pos;
  vs_out.FragPosLightSpace = light_world_to_clip_matrix * vec4(world_pos, 1.0);
  vs_out.fV = camera_position - world_pos;
  // add 模拟太阳光：光照方向 = 光源本身的方向向量
  vs_out.fL = light_position;

  gl_Position =
	  vertex_view_to_projection * vertex_world_to_view * vec4(world_pos, 1.0);
  // if ((v_InstanceID * 25 + v_VertexID) % 15 == 0 && lables == 1) {
  //   gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
  // }
}
