#version 410

layout(location = 0) in vec3 vertex;

uniform mat4 vertex_world_to_clip; // View * Projection

out vec3 localPos; // 传给 frag 用来计算方向

void main() {
	localPos = vertex;

	// 移除 View 矩阵的平移分量后计算位置
	gl_Position = vertex_world_to_clip * vec4(vertex, 1.0);
}
