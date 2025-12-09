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
  return sunColor;
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

// 计算相位的函数（Henyey-Greenstein），用于模拟米氏散射
// g: 散射各向异性 (-1 到
// 1)。正值表示向前散射（逆光看更亮），适合模拟空气中的灰尘。 cosTheta:
// 视线方向和光线方向的夹角余弦值
float GetMiePhase(float g, float cosTheta) {
  float g2 = g * g;
  float num = 1.0 - g2;
  float denom = 1.0 + g2 - 2.0 * g * cosTheta;
  return num / (4.0 * 3.14159265 * pow(denom, 1.5));
}

// 用于在体积计算中查询阴影的辅助函数
// 类似于你之前的 calculateLight，但去掉了 bias 和 PCF 以提高性能
float GetVolumetricShadow(vec3 worldPos, mat4 lightMatrix,
                          sampler2D shadowMap) {
  vec4 clipPos = lightMatrix * vec4(worldPos, 1.0);
  vec3 projCoords = clipPos.xyz / clipPos.w;
  projCoords = projCoords * 0.5 + 0.5;

  if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 ||
      projCoords.y < 0.0 || projCoords.y > 1.0)
    return 1.0; // 超出范围视为被照亮

  float closestDepth = texture(shadowMap, projCoords.xy).r;
  float currentDepth = projCoords.z;

  // 这里不需要 bias，或者用一个极小的 bias，因为体积是连续的
  return currentDepth > closestDepth ? 0.0 : 1.0;
}

// 主函数
vec3 CalculateVolumetricFog(
    vec3 worldPos,       // 当前像素对应的世界坐标（可能是远平面上的点）
    vec3 cameraPos,      // 摄像机位置
    vec3 sunDir,         // 指向太阳的方向 (normalize(sunPos - worldZero))
    vec3 sunCol,         // 太阳光颜色强度
    mat4 lightMatrix,    // 光照视图投影矩阵
    sampler2D shadowMap, // 阴影贴图
    float sceneDepth     // 场景线性深度（用于提前停止步进）
) {
  // --- 配置参数 ---
  int STEPS = 16;             // 步进次数。越高越好，但越卡。8/16/32/64
  float MAX_DISTANCE = 200.0; // 光柱的最大可见距离。
  float FOG_DENSITY = 0.05;   // 雾的密度。控制光柱的强度。
  float SCATTERING_G = 0.8;   // 散射系数。接近 1 表示逆光更强。

  // --- 初始化射线 ---
  vec3 rayVector = worldPos - cameraPos;
  float rayLength = length(rayVector);
  vec3 rayDir = rayVector / rayLength;

  // 限制射线的最大长度，不能超过设定的最大距离，也不能穿透场景物体
  float targetDistance = min(rayLength, MAX_DISTANCE);

  // 如果你有线性的场景深度值，也可以用它来截断射线
  // targetDistance = min(targetDistance, sceneDepth);

  float stepLength = targetDistance / float(STEPS);
  vec3 currentPos = cameraPos;

  // --- 累加变量 ---
  vec3 accumulatedLight = vec3(0.0);
  float transmittance = 1.0; // 透射率，初始为 1 (完全透明)

  // --- 计算相位函数 ---
  // 视线方向是射线的反方向
  float cosTheta = dot(rayDir, sunDir);
  float phaseVal = GetMiePhase(SCATTERING_G, cosTheta);

  // --- 光线步进循环 ---
  for (int i = 0; i < STEPS; ++i) {
    // 1. 判断当前点是否在阴影中
    // 这是形成光柱最关键的一步！
    float shadow = GetVolumetricShadow(currentPos, lightMatrix, shadowMap);

    // 如果被照亮 (shadow > 0)
    if (shadow > 0.001) {
      // 2. 计算该点的散射光
      // 入射光 = 太阳颜色 * 阴影遮挡(1或0) * 沿途的衰减(透射率)
      vec3 incomingLight = sunCol * shadow * transmittance;

      // 散射到摄像机的光 = 入射光 * 密度 * 步长 * 相位
      accumulatedLight += incomingLight * FOG_DENSITY * stepLength * phaseVal;
    }

    // 3. 根据比尔-朗伯定律计算衰减 (雾自身会遮挡后面的光)
    transmittance *= exp(-FOG_DENSITY * stepLength);

    // 优化：如果透射率太低（太黑了），就提前退出
    if (transmittance < 0.01)
      break;

    // 4. 前进一步
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
    vec3 sunColor = getSunColor(L.y) * 5;
    volumetricLight = CalculateVolumetricFog(
        fs_in.world_pos, camera_position,
        L,        // 假设 light_direction 是指向太阳的
        sunColor, // 例如 vec3(10.0) 强度要高一些
        light_world_to_clip_matrix, shadow_texture,
        1000.0 // 暂时传入一个很大的深度值，之后可以用真实的线性深度
    );
  }
  frag_color = vec4(sceneColor + volumetricLight, 1.0);
}
