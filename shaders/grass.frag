// grass.frag
#version 410

uniform sampler2D grass_alpha;
uniform sampler2D grass_texture;
uniform int is_leaves; // 虽然草可能不需要这个，但保留着防止报错

in VS_OUT {
  vec3 normal;
  vec2 texcoord;
  vec3 fV;
  vec3 fL;
} fs_in;

out vec4 frag_color;

void main() {
	// -----------------------------------------------------------
	// 1. Alpha 测试 (透贴剔除)
	// -----------------------------------------------------------
	vec4 mask = texture(grass_alpha, fs_in.texcoord);
	float luminance = 0.2126 * mask.r + 0.7152 * mask.g + 0.0722 * mask.b;
	if (luminance < 0.2) {
		discard;
	}

	// -----------------------------------------------------------
	// 2. 准备数据
	// -----------------------------------------------------------
	vec3 N = normalize(fs_in.normal);
	vec3 V = normalize(fs_in.fV);
	vec3 L = normalize(fs_in.fL);
	vec3 R = reflect(-L, N);
	
	vec4 albedoTexture = texture(grass_texture, fs_in.texcoord);

	// -----------------------------------------------------------
	// 3. 动态天空颜色
	// -----------------------------------------------------------
	float sunHeight = L.y;

	vec3 noonSun    = vec3(1.0, 0.98, 0.9);
	vec3 sunsetSun  = vec3(1.0, 0.4, 0.1);
	vec3 nightSun   = vec3(0.0, 0.0, 0.0);

	vec3 noonAmb    = vec3(0.4, 0.4, 0.45);
	vec3 sunsetAmb  = vec3(0.3, 0.2, 0.2);
	vec3 nightAmb   = vec3(0.02, 0.02, 0.05);

	vec3 sunColor;
	vec3 skyAmbient;

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
	
	// 环境光
	vec3 ambient = skyAmbient * albedoTexture.rgb * 0.5;

	// 漫反射
	// 双面光照技巧：草是薄片，背面也应该受光
	// 简单的双面光照：用 abs(dot(L, N)) 或者把法线翻转
	// 这里先用标准的 max(dot)
	float diff = max(dot(L, N), 0.0);
	vec3 diffuse = diff * sunColor * albedoTexture.rgb;

	// 高光
	float spec = pow(max(dot(V, R), 0.0), 10.0);
	vec3 specular = spec * sunColor * 0.1; // 强度很低

	// -----------------------------------------------------------
	// 5. 输出
	// -----------------------------------------------------------
	frag_color = vec4(ambient + diffuse + specular, 1.0);
}
