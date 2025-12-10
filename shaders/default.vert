#version 410

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 texcoord;
layout(location = 3) in vec3 tangent;


layout(location = 7) in vec4 instanceMatrix1;
layout(location = 8) in vec4 instanceMatrix2;
layout(location = 9) in vec4 instanceMatrix3;
layout(location = 10) in vec4 instanceMatrix4;

uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_clip;
uniform vec3 light_position;
uniform vec3 camera_position;

uniform float wind_strength;
uniform float elapsed_time_s;
uniform int is_leaves;

out VS_OUT {
  vec3 normal;
  vec2 texcoord;
  vec3 fV;
  vec3 fL;
  mat3 TBN; // 输出 TBN
}
vs_out;

void main() {
	vec3 newPos = vertex;
	if (wind_strength > 0.0) { // 有风才计算
		if (is_leaves == 1) {
			// === 树叶逻辑 ===
			// 1. 高频颤动 (模拟风吹叶子哗哗响)
			float flutter = sin(elapsed_time_s * 10.0 + vertex.x + vertex.y + vertex.z);
			float flutterAmt = 0.05 * wind_strength; // 颤动幅度
			newPos += flutter * flutterAmt;

			// 2. 整体随风摆动 (跟随树干)
			// 假设树高约 10 米，高度越高摆动越大
			float sway = sin(elapsed_time_s * 1.0 + vertex.x);
			float heightFactor = max(0.0, vertex.y - 2.0) * 0.02; // 2米以上才动
			newPos.x += sway * heightFactor * wind_strength;
		}
		else {
			// === 树干逻辑 ===
			// 树干很硬，只有轻微的低频摆动
			float sway = sin(elapsed_time_s * 1.0 + vertex.x);
			float heightFactor = max(0.0, vertex.y - 1.0) * 0.005; // 幅度极小
			newPos.x += sway * heightFactor * wind_strength;
		}
	}
	mat4 instanceMatrix = mat4(instanceMatrix1, instanceMatrix2, instanceMatrix3, instanceMatrix4);
	mat4 model_to_world = vertex_model_to_world * instanceMatrix;
	mat4 normal_model_to_world = transpose(inverse(model_to_world));

	vs_out.texcoord = texcoord.xy;

	// --- 计算 TBN  ---
	vec3 T = normalize(mat3(normal_model_to_world) * tangent);
	vec3 N = normalize(mat3(normal_model_to_world) * normal);
	T = normalize(T - dot(T, N) * N);
	vec3 B = cross(N, T);
	vs_out.TBN = mat3(T, B, N);
  // ------------------------------------

	vs_out.normal = vec3(normal_model_to_world * vec4(normal, 0.0));
	vs_out.fV = camera_position - vec3(model_to_world * vec4(newPos, 1.0));
	// add 模拟太阳光：光照方向 = 光源本身的方向向量
	vs_out.fL = light_position;
	gl_Position = vertex_world_to_clip * model_to_world * vec4(newPos, 1.0);
}


