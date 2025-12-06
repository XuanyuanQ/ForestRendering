#version 410

uniform vec3 light_position;

in VS_OUT {
	vec3 normal;
	vec2 texcoord;
	vec3 fV;
	vec3 fL;
} fs_in;
fs_in;

uniform vec3 diffuse_colour;
uniform vec3 specular_colour;
uniform vec3 ambient_colour;
uniform float shininess_value;
uniform int use_normal_mapping;
uniform samplerCube cubemap;

uniform sampler2D diffuse_texture;
uniform sampler2D specular_texture;
uniform sampler2D normal_texture;
uniform sampler2D waveNormal_texture;

out vec4 frag_color;

void main() {
	// 1. 准备向量
	vec3 N = normalize(fs_in.normal);
	vec3 V = normalize(fs_in.fV);
	vec3 L = normalize(fs_in.fL); // 指向太阳的向量
	vec3 R = reflect(-L, N);

	// 2. 采样纹理
	// 地面很大，必须让纹理重复平铺，否则会拉伸得很模糊
	vec4 albedoTexture = texture(diffuse_texture, fs_in.texcoord * 20.0);

	// -----------------------------------------------------------
	// 3. 动态天空颜色
	// -----------------------------------------------------------
	// 通过光照向量的 Y 分量判断太阳高度
	float sunHeight = L.y;

	// 定义天空颜色 (正午、日落、夜晚)
	vec3 noonSun    = vec3(1.0, 0.98, 0.9);
	vec3 sunsetSun  = vec3(1.0, 0.4, 0.1);
	vec3 nightSun   = vec3(0.0, 0.0, 0.0);

	// 定义环境光颜色
	vec3 noonAmb    = vec3(0.4, 0.4, 0.45);
	vec3 sunsetAmb  = vec3(0.3, 0.2, 0.2);
	vec3 nightAmb   = vec3(0.02, 0.02, 0.05);

	vec3 sunColor;
	vec3 skyAmbient;

	// 混合逻辑
	if (sunHeight > 0.2) {
		float t = (sunHeight - 0.2) / 0.8;
		sunColor = mix(sunsetSun, noonSun, t);
		skyAmbient = mix(sunsetAmb, noonAmb, t);
	} else if (sunHeight > -0.1) {
		float t = (sunHeight + 0.1) / 0.3;
		sunColor = mix(nightSun, sunsetSun, t);
		skyAmbient = mix(nightAmb, sunsetAmb, t);
	} else {
		sunColor = nightSun;
		skyAmbient = nightAmb;
	}

	// -----------------------------------------------------------
	// 4. 光照计算
	// -----------------------------------------------------------

	// A. 环境光
	// 混合：天空环境色 * 地面纹理颜色
	vec3 ambientFinal = skyAmbient * albedoTexture.rgb;

	// B. 漫反射
	// 混合：漫反射强度 * 阳光颜色 * 地面纹理颜色
	float diff = max(dot(L, N), 0.0);
	vec3 diffuseFinal = diff * sunColor * albedoTexture.rgb;

	// C. 高光
	float spec = pow(max(dot(V, R), 0.0), 5.0); // shininess 设为 5.0
	vec3 specularFinal = spec * sunColor * 0.1; // 强度设为 0.1

	// -----------------------------------------------------------
	// 5. 输出
	// -----------------------------------------------------------
	frag_color = vec4(ambientFinal + diffuseFinal + specularFinal, 1.0);
}
