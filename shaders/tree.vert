#version 410 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_texcoord;

// --- 关键修改 ---
// 实例化矩阵占用 4 个位置 (location 3, 4, 5, 6)
// 因为 OpenGL 的顶点属性最大是 vec4，mat4 需要 4 个 vec4
layout (location = 3) in mat4 instance_matrix;

uniform mat4 view_projection; // VP 矩阵

out vec3 vs_normal;
out vec2 vs_texcoord;

void main()
{
	// 使用 instance_matrix 而不是 uniform 的 model 矩阵
	gl_Position = view_projection * instance_matrix * vec4(in_position, 1.0);
	
	// 简单的法线传递 (如果需要光照，这里也要用 instance_matrix 变换法线)
	vs_normal = mat3(instance_matrix) * in_normal;
	vs_texcoord = in_texcoord.xy;
}
