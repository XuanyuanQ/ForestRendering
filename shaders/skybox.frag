#version 410

in vec3 localPos;
out vec4 frag_color;

uniform vec3 light_position; // 太阳方向

vec3 getSunColor(float sunHeight) {
  vec3 noonSun = vec3(1.0, 0.6, 0.0);
  vec3 sunsetSun = vec3(1.0, 0.7, 0.3);
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

void main() {
  // 1. 计算观察方向 (归一化)
  vec3 V = normalize(localPos);
  // 太阳方向 (归一化)
  vec3 L = normalize(light_position);

  vec3 sunDiskColor = getSunColor(L.y) * 1.0;
  // -----------------------------------------------------------
  // 2. 背景渐变色
  // -----------------------------------------------------------
  float sunHeight = L.y;

  // 稍微调整一下颜色，让天空看起来更通透
  vec3 noonTop = vec3(0.2, 0.5, 0.9);   // 中午头顶蓝
  vec3 noonHoriz = vec3(0.6, 0.8, 1.0); // 中午地平线白

  vec3 sunsetTop = vec3(0.2, 0.2, 0.4);   // 日落头顶紫
  vec3 sunsetHoriz = vec3(1.0, 0.5, 0.1); // 日落地平线橙

  vec3 nightTop = vec3(0.0, 0.0, 0.1);   // 晚上头顶黑
  vec3 nightHoriz = vec3(0.0, 0.0, 0.2); // 晚上地平线深蓝

  // 混合因子
  vec3 skyTop, skyHoriz;
  float t_day = 0.0;

  if (sunHeight > 0.2) {
    float t = clamp((sunHeight - 0.2) / 0.8, 0.0, 1.0);
    skyTop = mix(sunsetTop, noonTop, t);
    skyHoriz = mix(sunsetHoriz, noonHoriz, t);
  } else if (sunHeight > -0.2) {
    float t = clamp((sunHeight + 0.2) / 0.4, 0.0, 1.0);
    skyTop = mix(nightTop, sunsetTop, t);
    skyHoriz = mix(nightHoriz, sunsetHoriz, t);
  } else {
    skyTop = nightTop;
    skyHoriz = nightHoriz;
  }

  // 计算当前像素的仰角 (0=地平线, 1=头顶)
  // 使用 V.y 做垂直渐变
  float gradient = clamp(V.y, 0.0, 1.0);
  // 加上 pow 让地平线雾气感更强
  gradient = pow(gradient, 0.5);

  // -----------------------------------------------------------
  // 3. 画太阳 (Sun Disk)
  // -----------------------------------------------------------
  // 计算视线和太阳的夹角余弦
  float sunDot = dot(V, L);

  // 如果夹角非常小 (接近 1.0)，说明看的是太阳
  // 0.999 决定了太阳的大小
  float sunMask = step(0.999, sunDot);

  vec3 skyColor = mix(skyHoriz, skyTop, gradient) * (1 - sunMask);
  // skyColor = vec3(0.0) * (1 - sunMask);
  // 太阳颜色 (亮黄白)

  // 叠加太阳 (只在白天和日落显示)
  skyColor += sunMask * sunDiskColor;

  frag_color = vec4(skyColor, 1.0);
}
