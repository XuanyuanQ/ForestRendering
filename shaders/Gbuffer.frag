#version 410
uniform sampler2D txture_alpha;
uniform sampler2D txture;
uniform sampler2D normals_texture;
uniform sampler2D shadow_texture;

uniform int lables; // 0-terrain,1-leaves,2-bark,3-grass
uniform mat4 light_world_to_clip_matrix;

// 摄像机位置
uniform vec3 camera_position;

// 光源参数
uniform vec3 light_position;
uniform vec3 light_direction;

// 屏幕反向分辨率
uniform vec2 inverse_screen_resolution;

in VS_OUT {
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
fs_in;
out vec4 frag_color;

void calculateGrass(in vec4 albedoTexture, in vec3 L, in vec3 V, in vec3 N,
                    out vec3 ambient, out vec3 diffuse, out vec3 specular) {
  // -----------------------------------------------------------
  // 3. 动态天空颜色
  // -----------------------------------------------------------
  float sunHeight = L.y;
  vec3 R = reflect(-L, N);

  vec3 noonSun = vec3(1.0, 0.98, 0.9);
  vec3 sunsetSun = vec3(1.0, 0.4, 0.1);
  vec3 nightSun = vec3(0.0, 0.0, 0.0);

  vec3 noonAmb = vec3(0.4, 0.4, 0.45);
  vec3 sunsetAmb = vec3(0.3, 0.2, 0.2);
  vec3 nightAmb = vec3(0.02, 0.02, 0.05);

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
  ambient = skyAmbient * albedoTexture.rgb * 0.5;

  // 漫反射
  // 双面光照技巧：草是薄片，背面也应该受光
  // 简单的双面光照：用 abs(dot(L, N)) 或者把法线翻转
  // 这里先用标准的 max(dot)
  float diff = max(dot(L, N), 0.0);
  diffuse = diff * sunColor * albedoTexture.rgb;

  // 高光
  float spec = pow(max(dot(V, R), 0.0), 10.0);
  specular = spec * sunColor * 0.1; // 强度很低
}

// --------------------------------------
// 地形光照计算
// --------------------------------------
void calculateTrees(in float shininess, in float specularStrength,
                    in vec4 albedoTexture, in vec3 L, in vec3 V, in vec3 N,
                    out vec3 ambient, out vec3 diffuse, out vec3 specular) {

  // 1. 获取太阳高度 (0.0是地平线，1.0是头顶)
  // 我们用 L.y (光照向量的垂直分量) 来判断
  float sunHeight = L.y;
  vec3 R = reflect(-L, N);

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
  ambient = skyAmbient * albedoTexture.rgb;

  // 漫反射 = 漫反射强度 * 动态阳光色 * 材质固有色
  float diff = max(dot(L, N), 0.0);
  //   diff = 1.0;
  //   diffuse = diff * sunColor * albedoTexture.rgb;
  diffuse = diff * sunColor * albedoTexture.rgb;

  // 高光 = 高光强度 * 动态阳光色
  float spec = pow(max(dot(V, R), 0.0), shininess);
  specular = spec * specularStrength * sunColor;
}

// --------------------------------------
// 树木光照计算
// --------------------------------------
void calculateTerrain(in vec4 albedoTexture, in vec3 L, in vec3 V, in vec3 N,
                      out vec3 ambient, out vec3 diffuse, out vec3 specular) {
  // -----------------------------------------------------------
  // 3. 动态天空颜色
  // -----------------------------------------------------------
  // 通过光照向量的 Y 分量判断太阳高度
  float sunHeight = L.y;
  vec3 R = reflect(-L, N);

  // 定义天空颜色 (正午、日落、夜晚)
  vec3 noonSun = vec3(1.0, 0.98, 0.9);
  vec3 sunsetSun = vec3(1.0, 0.4, 0.1);
  vec3 nightSun = vec3(0.0, 0.0, 0.0);

  // 定义环境光颜色
  vec3 noonAmb = vec3(0.4, 0.4, 0.45);
  vec3 sunsetAmb = vec3(0.3, 0.2, 0.2);
  vec3 nightAmb = vec3(0.02, 0.02, 0.05);

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
  ambient = skyAmbient * albedoTexture.rgb;

  // B. 漫反射
  // 混合：漫反射强度 * 阳光颜色 * 地面纹理颜色
  float diff = max(dot(L, N), 0.0);
  diffuse = diff * sunColor * albedoTexture.rgb;

  // C. 高光
  float spec = pow(max(dot(V, R), 0.0), 5.0); // shininess 设为 5.0
  specular = spec * sunColor * 0.1;           // 强度设为 0.1
}

float calculateLight(vec3 world_pos, mat4 light_projection,
                     vec2 shadowmap_texel_size) {
  //   float light_intensity = 20.0 * 100.0 * 100.0;
  //   float light_angle_falloff = radians(37.0);

  //   vec2 texcoord = gl_FragCoord.xy * inverse_screen_resolution;

  //   float distance_falloff = 1.0 / max(0.0001, length(L) * length(L));

  //   float cosTheta = dot(normalize(light_direction), -normalize(L));
  //   float cutoff = cos(radians(40.0));
  //   float angle_falloff = pow(smoothstep(cutoff, 1.0, cosTheta), 2.0);

  // Shadow mapping PCF
  vec4 clip_pos = light_projection * vec4(world_pos, 1.0);
  vec4 ndc_pos = clip_pos / clip_pos.w;
  vec2 light_uv = ndc_pos.xy * 0.5 + 0.5;
  float current_depth = clamp(-1.0, 1.0, ndc_pos.r);

  float light_d = 0.0;
  for (int i = -2; i <= 2; ++i) {
    for (int j = -2; j <= 2; ++j) {
      vec2 offset = vec2(i, j) * shadowmap_texel_size;
      float d = texture(shadow_texture, light_uv + offset).r;

      if (clamp(0.0, 1.0, d) * 2.0 - 1.0 + 0.005 > current_depth)
        light_d += 1.0;
    }
  }
  float lightpcf = light_d / 15.0;
  //   float d = texture(shadow_texture, light_uv).r;
  //   float lightpcf = 1.0;
  //   if (clamp(0.0, 1.0, d) * 2.0 - 1.0 < current_depth - 0.005) {
  //     lightpcf = 0.0;
  //   }
  return lightpcf;
}

void main() {

  if (lables == 3) {
    vec4 mask = texture(txture_alpha, fs_in.texcoord);
    float luminance = 0.2126 * mask.r + 0.7152 * mask.g + 0.0722 * mask.b;
    if (luminance < 0.2) {
      discard;
    }
  }
  if (lables == 1) {
    float mask = texture(txture_alpha, fs_in.texcoord).r;
    if (mask < 0.5)
      discard;
  }

  vec4 albedoTexture;
  float shininess;
  float specularStrength;

  vec3 L = normalize(fs_in.fL);
  vec3 V = normalize(fs_in.fV);
  albedoTexture = texture(txture, fs_in.texcoord);
  vec2 shadowmap_texel_size = 1.0 / textureSize(shadow_texture, 0);
  float light_pcf = calculateLight(
      fs_in.world_pos.xyz, light_world_to_clip_matrix, shadowmap_texel_size);
  vec4 clip = light_world_to_clip_matrix * vec4(fs_in.world_pos.xyz, 1.0);
  vec3 ndc = (clip.xyz / clip.w);
  //   vec3 projCoords = vec3(1.0);
  //   vec4 fragPosLightSpace = fs_in.fragPosLightSpace;
  //   projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
  //   projCoords = projCoords * 0.5 + 0.5;
  //   ndc.z = projCoords.z;
  vec3 finalNormal;
  if (lables == 1) {
    vec3 rawNormal = texture(normals_texture, fs_in.texcoord).rgb;

    rawNormal = rawNormal * 2.0 - 1.0;
    rawNormal.xy *= 0.5;
    finalNormal = normalize(fs_in.TBN * rawNormal);
    shininess = 30.0;
    specularStrength = 0.3;
  } else if (lables == 2) {
    vec3 rawNormal = texture(normals_texture, fs_in.texcoord).rgb;

    rawNormal = rawNormal * 2.0 - 1.0;
    rawNormal.xy *= 1.2;
    finalNormal = normalize(fs_in.TBN * rawNormal);
    shininess = 5.0;
    specularStrength = 0.02;
    vec3 woodTint = vec3(1.0, 0.8, 0.6);
    albedoTexture.rgb *= woodTint;

  } else {
    if (lables == 0) {
      albedoTexture = texture(txture, fs_in.texcoord * 20.0);
    }
    finalNormal = normalize(fs_in.normal);
  }
  //   finalNormal = normalize(fs_in.normal);
  vec3 diffuse, specular, ambient;
  diffuse = vec3(1.0);
  specular = vec3(1.0);
  ambient = vec3(1.0);
  if (lables == 3) {
    calculateGrass(albedoTexture, L, V, finalNormal, ambient, diffuse,
                   specular);
  } else if (lables == 1 || lables == 2) {
    calculateTrees(shininess, specularStrength, albedoTexture, L, V,
                   finalNormal, ambient, diffuse, specular);

  } else {
    calculateTerrain(albedoTexture, L, V, finalNormal, ambient, diffuse,
                     specular);
  }

  //   light_pcf = 1.0;
  frag_color = vec4((light_pcf * (diffuse + specular) + ambient), 1.0);
  //   frag_color = vec4(vec3(light_pcf), 1.0);
}
