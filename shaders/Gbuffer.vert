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

//接收时间与风力
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
    scale = 0.015;
  }
  if (lables == 1 || lables == 2) {
    scale = 0.4;
  }
  vs_out.normal = vec3(vs_out.normal_model_to_world * vec4(normal, 0.0));

  // --- 计算 TBN  ---
  vec3 T = normalize(mat3(vs_out.normal_model_to_world) * tangent);
  vec3 N = normalize(mat3(vs_out.normal_model_to_world) * normal);
  T = normalize(T - dot(T, N) * N);
  vec3 B = cross(N, T);
  vs_out.TBN = mat3(T, B, N);
  
  float time = 1.0;
  // 计算两个波形叠加
  float wave1 = waveFun(time, 1.0, 0.2, 0.5, 2.0, vec2(-1.0, 0.0), vertex);
  float wave2 = waveFun(time, 0.5, 0.4, 1.3, 2.0, vec2(-0.7, 0.7), vertex);

  // 应用高度偏移
  // 注意：这里修改的是
  // worldPos.y，这样不仅位置变了，后续的光照计算也会基于这个新高度
  float heightOffset = 1.0 * (wave1 + wave2);
  world_pos = vec3(model_to_world * vec4(vertex * scale, 1.0));
  world_pos.y += heightOffset;
	
	
  //添加风吹
	if (wind_strength > 0.0) {
		vec3 windDir = normalize(vec3(1.0, 0.0, 0.5)); // 统一风向
		switch (lables) {
					case 3: // Grass
					{
						float wave = sin(elapsed_time_s * 3.0 + world_pos.x * 2.0 + world_pos.z * 1.0);
						world_pos.x += wave * wind_strength * 0.2;
						world_pos.z += cos(elapsed_time_s * 2.5 + world_pos.x * 3.0) * wind_strength * 0.1;
						break;
					}
					
					case 1: // Leaves
					{
						// -------------------------------------------------------------
							// 1. 去同步
							float treePhase = world_pos.x * 0.7 + world_pos.z * 0.3;

							// -------------------------------------------------------------
							// 2. 主摇摆
							// -------------------------------------------------------------
							// 频率低 (1.0)，幅度大。
							// 叠加两个不同频率的 sin 波，模拟风的“阵风”感
							float sway = sin(elapsed_time_s * 1.0 + treePhase)
									   + sin(elapsed_time_s * 0.4 + treePhase * 0.8) * 0.5;

							// -------------------------------------------------------------
							// 3. 叶片颤动
							// -------------------------------------------------------------
							// 频率高 (15.0)，幅度小。
							// 使用 vertex (局部坐标) 参与计算，让同一棵树上的不同叶子动得不一样。
							float flutter = sin(elapsed_time_s * 15.0 + vertex.x * 10.0 + vertex.y * 10.0 + vertex.z * 10.0);

							// -------------------------------------------------------------
							// 4. 弯曲权重
							// -------------------------------------------------------------
							// 实现树根 (y=0) 完全不动，中间动一点，树梢动得最厉害
							float h = max(0.0, vertex.y);
							float bendFactor = pow(h, 1.5);

							// -------------------------------------------------------------
							// 5. 合成最终运动
							// -------------------------------------------------------------
							// A. 整体倒向风向
							vec3 swayOffset = windDir * sway * bendFactor * 0.01 * wind_strength;
							
							// B. 叶子乱颤
							vec3 flutterOffset = vec3(flutter) * bendFactor * 0.02 * wind_strength;

							world_pos += swayOffset + flutterOffset;
							
							break;
					}

//					case 2: // Bark
//					{
//						float bendFactor = max(0.0, vertex.y) * 0.05 * wind_strength;
//						float sway = sin(elapsed_time_s * 1.5 + world_pos.x);
//						float windForce = bendFactor + (sway * bendFactor * 0.5);
//						
//						world_pos += windDir * windForce;
//						break;
//					}
					
					default:
						break;
				}
	  }
	
  // if (lables == 0) {

  // }
  vs_out.ndc = vertex_view_to_projection * vertex_world_to_view * model_to_world * vec4(world_pos, 1.0);
  vs_out.world_pos = world_pos;
  vs_out.FragPosLightSpace = light_world_to_clip_matrix * vec4(world_pos, 1.0);
  vs_out.fV = camera_position - world_pos;
  // add 模拟太阳光：光照方向 = 光源本身的方向向量
  vs_out.fL = light_position;

  gl_Position = vertex_view_to_projection * vertex_world_to_view * vec4(world_pos, 1.0);
}
