#version 410

uniform sampler2D diffuse_texture;
uniform int has_diffuse_texture;
uniform int has_normal_texture;

uniform sampler2D leaves_alpha;
uniform sampler2D maple_bark;
uniform sampler2D maple_leaf;
uniform int is_leaves;
uniform sampler2D maple_leaf_normal;
uniform sampler2D maple_bark_normal;

in VS_OUT {
  vec3 normal;
  vec2 texcoord;
  vec3 fV;
  vec3 fL;
  mat3 TBN;
}
fs_in;

out vec4 frag_color;

void main() {
  // 1. 准备变量
  vec3 N;
  vec4 albedoTexture;
  float shininess;
  float specularStrength;

  if (is_leaves != 0) {
    // === 树叶逻辑 (保持不变) ===
    float mask = texture(leaves_alpha, fs_in.texcoord).r;
    if (mask < 0.5)
      discard;

    albedoTexture = texture(maple_leaf, fs_in.texcoord);
    vec3 rawNormal = texture(maple_leaf_normal, fs_in.texcoord).rgb;
    rawNormal = rawNormal * 2.0 - 1.0;
    rawNormal.xy *= 0.5;
    N = normalize(fs_in.TBN * rawNormal);

    shininess = 30.0;
    specularStrength = 0.3;
  } else {
    // === 树干逻辑  ===

    vec2 trunkUV = fs_in.texcoord * vec2(1.0, 1.0);
    albedoTexture = texture(maple_bark, trunkUV);

    // 提亮染色剂
    vec3 woodTint = vec3(1.0, 0.8, 0.6);
    albedoTexture.rgb *= woodTint;

    // 法线采样 (保持不变)
    vec3 rawNormal = texture(maple_bark_normal, trunkUV).rgb;
    rawNormal = rawNormal * 2.0 - 1.0;
    rawNormal.xy *= 1.2; // 暂未迁移
    N = normalize(fs_in.TBN * rawNormal);

    shininess = 5.0;
    specularStrength = 0.02;
  }

  // 2. 光照计算
  vec3 V = normalize(fs_in.fV);
  vec3 L = normalize(fs_in.fL);
  vec3 R = reflect(-L, N);

  // 1. 获取太阳高度 (0.0是地平线，1.0是头顶)
  // 我们用 L.y (光照向量的垂直分量) 来判断
  float sunHeight = L.y;

  // 2. 定义不同时刻的阳光颜色 (Light Color)
  vec3 noonSun = vec3(1.0, 0.98, 0.9);  // 中午：暖白
  vec3 sunsetSun = vec3(1.0, 0.4, 0.1); // 日落：橘红
  vec3 nightSun = vec3(0.0, 0.0, 0.0);  // 晚上：无光

  // 3. 定义不同时刻的环境光颜色 (Ambient Color)
  vec3 noonAmb = vec3(0.4, 0.4, 0.45);    // 中午环境：亮蓝灰
  vec3 sunsetAmb = vec3(0.3, 0.2, 0.2);   // 日落环境：暗红褐
  vec3 nightAmb = vec3(0.02, 0.02, 0.05); // 晚上环境：深蓝黑

  // 4. 根据高度混合颜色
  vec3 sunColor;
  vec3 skyAmbient;

  if (sunHeight > 0.2) {
    // 白天 -> 中午 (混合 日落色 和 中午色)
    float t = (sunHeight - 0.2) / 0.8;
    sunColor = mix(sunsetSun, noonSun, t);
    skyAmbient = mix(sunsetAmb, noonAmb, t);
  } else if (sunHeight > -0.1) {
    // 日落 -> 晚上 (混合 晚上色 和 日落色)
    float t = (sunHeight + 0.1) / 0.3;
    sunColor = mix(nightSun, sunsetSun, t);
    skyAmbient = mix(nightAmb, sunsetAmb, t);
  } else {
    // 纯晚上
    sunColor = nightSun;
    skyAmbient = nightAmb;
  }

  // -------------------------------------------------------------
  // 5. 应用光照 (使用动态计算出的 sunColor 和 skyAmbient)
  // -------------------------------------------------------------

  // 环境光 = 动态环境色 * 材质固有色
  vec3 ambient = skyAmbient * albedoTexture.rgb;

  // 漫反射 = 漫反射强度 * 动态阳光色 * 材质固有色
  float diff = max(dot(L, N), 0.0);
  vec3 diffuse = diff * sunColor * albedoTexture.rgb;

  // 高光 = 高光强度 * 动态阳光色
  float spec = pow(max(dot(V, R), 0.0), shininess);
  vec3 specular = spec * specularStrength * sunColor;

  // 输出
  frag_color = vec4(ambient + diffuse + specular, 1.0);
}
