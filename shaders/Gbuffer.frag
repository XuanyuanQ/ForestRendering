#version 410
uniform sampler2D txture_alpha;
uniform sampler2D txture;
uniform sampler2D normals_texture;
uniform sampler2D shadow_texture;

uniform int lables; // 0-terrain,1-leaves,2-bark,3-grass
uniform int isApplyShadow;
uniform int isVolumetricLight;
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

vec3 getSunColor(float sunHeight) {
  vec3 noonSun = vec3(1.0, 0.98, 0.9);
  vec3 sunsetSun = vec3(1.0, 0.4, 0.1);
  vec3 nightSun = vec3(0.0, 0.0, 0.0);

  vec3 sunColor;
  if (sunHeight > 0.2) {
    // 白天 -> 中午 (混合 日落色 和 中午色)
    float t = (sunHeight - 0.2) / 0.8;
    sunColor = mix(sunsetSun, noonSun, t);

  } else if (sunHeight > -0.1) {
    // 日落 -> 晚上 (混合 晚上色 和 日落色)
    float t = (sunHeight + 0.1) / 0.3;
    sunColor = mix(nightSun, sunsetSun, t);

  } else {
    // 纯晚上
    sunColor = nightSun;
  }
  float factor = pow(150, sunHeight * 0.6 + 1.0);
  return sunColor * factor;
}

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

  ambient = ambient * 0.4;
  diffuse = diffuse * 0.3;
  specular = vec3(0.0, 0.38, 0.0) * 0.1; // 暂时规避加上specular无阴影
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
  diffuse = diff * sunColor * albedoTexture.rgb * 0.3;

  // C. 高光
  float spec = pow(max(dot(V, R), 0.0), 50.0); // shininess 设为 5.0
  specular = spec * sunColor * 0.1;            // 强度设为 0.1
  ambient = ambient * 0.4;
  diffuse = diffuse * 0.3;
  specular = vec3(0.5, 0.38, 0.2) * 0.1; // 暂时规避加上specular无阴影
}

float calculateLight(vec3 world_pos, mat4 light_projection,
                     vec2 shadowmap_texel_size) {
  vec4 clip_pos = light_projection * vec4(world_pos, 1.0);

  // 1. 计算 NDC 坐标
  vec3 projCoords = clip_pos.xyz / clip_pos.w;

  // 2. 变换到 [0, 1] 区间 (用于纹理采样和深度比较)
  // 这一步把 [-1, 1] 的 NDC 深度变成了 [0, 1] 的深度
  projCoords = projCoords * 0.5 + 0.5;

  // 3. 解决超出视锥体的边界问题
  if (projCoords.z > 1.0)
    return 1.0;

  float current_depth = projCoords.z;

  // 4. 计算 Bias (根据你的场景调整，0.005 对于正交投影通常是安全的)
  float bias = 0.005;

  float shadow_sum = 0.0;

  // PCF 5x5 采样
  for (int i = -2; i <= 2; ++i) {
    for (int j = -2; j <= 2; ++j) {
      // 采样 ShadowMap (值在 0.0 到 1.0 之间)
      float closest_depth =
          texture(shadow_texture,
                  projCoords.xy + vec2(i, j) * shadowmap_texel_size)
              .r;

      // 比较逻辑：
      // 如果 "当前深度 - bias" > "最近深度"，说明我在后面 -> 阴影 (1.0)
      // 否则 -> 亮部 (0.0)
      if (current_depth - bias > closest_depth)
        shadow_sum += 1.0;
    }
  }

  //   计算平均阴影值
  //   shadow_sum 是有多少个点说我是阴影 如果 25 个点都说阴影，shadow_factor =
  //   1.0
  float shadow_factor = shadow_sum / 25.0;
  return 1.0 - shadow_factor;
  //   shadow_sum = texture(shadow_texture, projCoords.xy).r;
  // 返回光照强度 (1.0 - 阴影)
  //   return shadow_sum;
}

// ==================================================================================
// 体积光计算核心函数
// ==================================================================================

float Random(vec2 co) {
  return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

// 米氏散射相位函数
float GetMiePhase(float g, float cosTheta) {
  // 针对极高 G 值的数值稳定性优化
  float g2 = g * g;
  float num = 1.0 - g2;
  float denom = 1.0 + g2 - 2.0 * g * cosTheta;
  // 防止分母为 0
  denom = max(denom, 0.0001);
  return num / (4.0 * 3.14159265 * pow(denom, 1.5));
}

// 核心计算函数
vec3 CalculateVolumetricFog(
    vec3 worldPos, vec3 cameraPos, vec3 sunDir,
    vec3 sunHighIntensityColor, // 【重点】传入一个强度极高的太阳颜色
                                // (例如原强度 * 100)
    mat4 lightMatrix, sampler2D shadowMap, vec2 screenPos) {
  // --- 【参数调优区】 ---
  int STEPS = 32; // 步数，越多越好
  float MAX_DISTANCE = 150.0;

  // 1. 雾的“绝对天花板”和“地板”高度
  // 假设你的树高 20米。
  float FOG_BOTTOM = 0.0; // 地面从 0 开始有雾
  float FOG_TOP =
      12.0; // 【关键】超过 12米（树冠中上部）彻底没雾！确保树顶不白。

  // 2. 基础密度
  float BASE_DENSITY = 0.02;

  // 3. 极致的各向异性
  // 【关键】参考图那种锋利感，需要 G 无限接近 1.0。试试 0.99甚至0.995
  float SCATTERING_G = 0.99;

  // --- 初始化射线 ---
  vec3 rayVector = worldPos - cameraPos;
  float rayLength = length(rayVector);
  vec3 rayDir = rayVector / rayLength;
  float targetDistance = min(rayLength, MAX_DISTANCE);
  float stepLength = targetDistance / float(STEPS);

  // Dithering 抖动
  float jitter = Random(screenPos + vec2(sin(sunDir.x), cos(sunDir.z)));
  vec3 currentPos = cameraPos + rayDir * stepLength * jitter;

  vec3 accumulatedLight = vec3(0.0);
  float transmittance = 1.0;

  float cosTheta = dot(rayDir, sunDir);
  float phaseVal = GetMiePhase(SCATTERING_G, cosTheta);

  for (int i = 0; i < STEPS; ++i) {
    // --- 【核心改进：基于高度的密度计算】 ---
    // 使用 smoothstep 创建一个平滑但有硬边界的雾层
    // 低于 FOG_BOTTOM 是 1.0，高于 FOG_TOP 绝对是 0.0
    float heightFactor = 1.0 - smoothstep(FOG_BOTTOM, FOG_TOP, currentPos.y);
    float currentDensity = BASE_DENSITY * heightFactor;

    // 只有当这里有雾，并且没被遮挡时，才计算光照
    if (currentDensity > 0.0001) {
      // 阴影测试 (此处省略具体实现，沿用你之前的 GetVolumetricShadow 逻辑即可)
      // 注意需确保 ShadowMap 采样正确
      vec4 clipPos = lightMatrix * vec4(currentPos, 1.0);
      vec3 projCoords = clipPos.xyz / clipPos.w;
      projCoords = projCoords * 0.5 + 0.5;
      float shadow = 1.0;
      if (projCoords.z < 1.0 && projCoords.x > 0.0 && projCoords.x < 1.0 &&
          projCoords.y > 0.0 && projCoords.y < 1.0) {
        float closestDepth = texture(shadowMap, projCoords.xy).r;
        // 假设 ShadowMap 背景是 1.0，物体深度 < 1.0
        if (projCoords.z > closestDepth + 0.001)
          shadow = 0.0; // 在阴影里
      }

      if (shadow > 0.01) {
        // 使用传入的高强度光计算
        vec3 incomingLight = sunHighIntensityColor * shadow * transmittance *
                             currentDensity * stepLength;
        accumulatedLight += incomingLight * phaseVal;
      }

      // 自身衰减
      transmittance *= exp(-currentDensity * stepLength);
    }

    if (transmittance < 0.01)
      break;
    currentPos += rayDir * stepLength;
  }

  return accumulatedLight;
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
  if (0 == isApplyShadow) {
    light_pcf = 1.0;
  }
  vec3 sceneColor = light_pcf * (diffuse + specular) + ambient;

  // --- 计算体积光 ---

  vec3 volumetricLight = vec3(0.0);
  if (isVolumetricLight == 1) {
    vec3 sunColor = getSunColor(L.y);
    volumetricLight = CalculateVolumetricFog(
        fs_in.world_pos, camera_position, L, sunColor,
        light_world_to_clip_matrix, shadow_texture, gl_FragCoord.xy);
  }

  frag_color = vec4(sceneColor + volumetricLight, 1.0);
}
