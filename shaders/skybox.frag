#version 410

in vec3 localPos;
out vec4 frag_color;
uniform float u_Time;
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

// 伪随机数生成器 (返回 0.0 - 1.0)
float hash12(vec2 p) {
  p = fract(p * vec2(5.3983, 5.4427));
  p += dot(p.yx, p.xy + vec2(21.5351, 14.3137));
  return fract(p.x * p.y * 95.4337);
}

// 伪随机二维向量生成器 (返回 -1.0 到 1.0)
vec2 hash22(vec2 p) {
  p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
  return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

// 绘制一层星星的核心函数 (输入是 vec2 uv)
vec3 LayerStars(vec2 uv, float scale, float seed) {
  vec2 gridUv = uv * scale;
  vec2 gridId = floor(gridUv);
  vec2 localUv = fract(gridUv) - 0.5;

  vec3 layerColor = vec3(0.0);

  // 3x3 邻域搜索，防止接缝
  for (int y = -1; y <= 1; y++) {
    for (int x = -1; x <= 1; x++) {
      vec2 neighborOffset = vec2(float(x), float(y));
      vec2 neighborGridId = gridId + neighborOffset;
      vec2 idWithSeed = neighborGridId + vec2(seed, seed * 12.34);

      vec2 starOffset = hash22(idWithSeed) * 0.4;
      vec2 starPosInLocal = neighborOffset + starOffset;

      float dist = length(localUv - starPosInLocal);

      // 星星形状
      float brightness = 1.0 - smoothstep(0.0, 0.1, dist);
      brightness = pow(brightness, 5.0);

      // 闪烁动画
      float randomVal = hash12(idWithSeed);
      float twinkleSpeed = 1.0 + randomVal * 4.0;
      float timeOffset = randomVal * 6.2831;
      float twinkle = sin(u_Time * twinkleSpeed + timeOffset) * 0.35 + 0.65;

      // 颜色倾向
      vec3 starTint = mix(vec3(0.8, 0.9, 1.0), vec3(1.0, 0.9, 0.7), randomVal);

      layerColor += starTint * brightness * twinkle;
    }
  }
  return layerColor;
}

// =================================================================================
// 新增核心函数：3D 方向转 2D UV (解决接缝的关键)
// =================================================================================
// 这个函数判断方向向量指向立方体的哪一个面，并将其投影到该面的 2D 平面上。
vec2 DirToRectilinearUV(vec3 dir) {
  vec3 absDir = abs(dir);
  float ma; // major axis (最大轴)
  vec2 uv;

  // 判断哪个轴的分量最大，决定投影到哪个面
  if (absDir.z >= absDir.x && absDir.z >= absDir.y) {
    ma = absDir.z;
    uv = dir.xy; // 投影到 XY 平面
  } else if (absDir.y >= absDir.x) {
    ma = absDir.y;
    uv = dir.xz; // 投影到 XZ 平面
  } else {
    ma = absDir.x;
    uv = dir.yz; // 投影到 YZ 平面
  }
  // 将坐标范围从 [-ma, ma] 映射到 [-1, 1]
  uv = uv / ma;

  // 核心 trick：为了让跨越面的网格能对齐，我们需要确保 UV 在面上是连续的。
  // 直接使用投影后的 UV 可能会在边缘产生拉伸或不匹配。
  // 这里不需要再映射到 [0,1]，保持 [-1,1] 的范围直接输入给 LayerStars
  // LayerStars 里的 floor() 和 fract() 会处理好周期性。
  return uv;
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
  int isNight = 0;
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
    isNight = 1;
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
  // 1. 获取标准化的 3D 方向向量

  vec3 dir = normalize(localPos);

  // 2. 【关键步骤】将 3D 方向转换为 2D UV
  // 我们需要乘上一个较大的基础缩放值，因为 LayerStars 里的 scale 是基于这个 UV
  // 的细分 如果不乘，星星会极其巨大。
  vec2 skyUV = DirToRectilinearUV(dir) * 2.0;
  vec3 sartColor = vec3(0.0);
  sartColor += LayerStars(skyUV, 10.0, 0.0);         // 大星星
  sartColor += LayerStars(skyUV, 20.0, 12.34) * 0.7; // 中星星
  sartColor += LayerStars(skyUV, 40.0, 56.78) * 0.5; // 密集小星星

  // 4. 加上深空背景色 (基于方向的 Y 分量做一点渐变)
  // 让头顶更黑，地平线稍微亮一点点
  vec3 bgColor = mix(vec3(0.005, 0.01, 0.04), vec3(0.02, 0.03, 0.08),
                     smoothstep(-0.5, 0.5, dir.y));
  sartColor += bgColor;

  // skyColor = vec3(0.0) * (1 - sunMask);
  // 太阳颜色 (亮黄白)

  // 叠加太阳 (只在白天和日落显示)
  skyColor += sunMask * sunDiskColor;
  if (isNight == 1) {
    skyColor += sartColor;
  }
  frag_color = vec4(skyColor, 1.0);
}
