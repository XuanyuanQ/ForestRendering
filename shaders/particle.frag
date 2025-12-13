#version 410 core
uniform sampler2D mask;
uniform sampler2D txture;

in VS_OUT {
  vec3 normal;
  vec2 texcoord;
  vec3 fV;
  vec3 fL;
}
fs_in;
flat in int v_InstanceID;
flat in int v_VertexID;
out vec4 frag_color;

float random(float seed) { return fract(sin(seed) * 43758.5453123); }
vec3 adjustSaturation(vec3 color, float saturation) {
  // 计算灰度值 (亮度)
  float grey = dot(color, vec3(0.299, 0.587, 0.114));
  // 在灰度和原色之间插值
  return mix(vec3(grey), color, saturation);
}

float adjustLeavesCol(vec3 stayGreen, vec3 turnOrange, vec3 turnRed) {

  float noise = 1.2 * random(float(v_InstanceID));

  // pow(noise, 2.5) 意味着大部分结果会比较小（偏绿），
  // 只有当 noise 接近 1.0 时，结果才会迅速变大（变红）。
  // 这符合“初秋”的感觉：大部分还是绿/黄，少部分红。
  float t = pow(noise, 3.5);

  vec3 finalTint;

  // 我们使用两个 mix 来实现三阶段过渡
  if (t < 0.5) {
    // 前 50% 的概率：在“墨绿”和“金橙”之间过渡
    // 将 t 从 [0.0, 0.5] 映射到 [0.0, 1.0]
    float subT = t * 2.0;
    finalTint = mix(stayGreen, turnOrange, subT);
  } else {
    // 后 50% 的概率：在“金橙”和“火红”之间过渡
    // 将 t 从 [0.5, 1.0] 映射到 [0.0, 1.0]
    float subT = (t - 0.5) * 2.0;
    finalTint = mix(turnOrange, turnRed, subT);
  }

  // --- 【明度微调】 ---
  // 红叶子通常比绿叶子看起来颜色更深重一点
  // 如果 t 比较大（偏红），稍微降低一点亮度
  float brightness = 1.0 - t * 0.2; // 0.8 ~ 1.0
  return finalTint * brightness;
}
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

void main() {
  float mask = texture(mask, fs_in.texcoord).r;
  if (mask < 0.5)
    discard;

  // vec3 rawNormal = texture(normals_texture, fs_in.texcoord).rgb;

  vec3 finalNormal = normalize(fs_in.normal);
  float shininess = 30.0;
  float specularStrength = 0.3;
  vec4 albedoTexture = texture(txture, fs_in.texcoord);
  vec3 stayGreen = vec3(0.8, 0.8, 0.6);
  vec3 turnOrange = vec3(1.4, 0.6, 0.3);
  vec3 turnRed = vec3(1.6, 0.3, 0.2);
  // 应用颜色
  albedoTexture.rgb *= adjustLeavesCol(stayGreen, turnOrange, turnRed);
  albedoTexture.rgb = adjustSaturation(albedoTexture.rgb, 1.1);
  vec3 diffuse, specular, ambient;
  diffuse = vec3(1.0);
  specular = vec3(1.0);
  ambient = vec3(1.0);
  vec3 L = normalize(fs_in.fL);
  vec3 V = normalize(fs_in.fV);
  calculateTrees(shininess, specularStrength, albedoTexture, L, V, finalNormal,
                 ambient, diffuse, specular);
  frag_color = vec4(ambient + diffuse + specular, 1.0);
}
