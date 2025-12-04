#version 410 core
in vec3 vs_normal;
out vec4 frag_color;

void main() {
	// 简单的光照或纯色
	vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
	float diff = max(dot(normalize(vs_normal), lightDir), 0.2);
	// 森林绿
	frag_color = vec4(0.1, 0.5, 0.1, 1.0) * diff;
}
