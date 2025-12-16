#version 410

in vec3 localPos;
out vec4 frag_color;
uniform float u_Time;
uniform vec3 light_position;

vec3 getSunColor(float sunHeight) {
  vec3 noonSun = vec3(1.0, 0.6, 0.0);
  vec3 sunsetSun = vec3(1.0, 0.7, 0.3);
  vec3 nightSun = vec3(0.0, 0.0, 0.0);

  vec3 sunColor;
  if (sunHeight > 0.2) {

    float t = (sunHeight - 0.2) / 0.8;
    sunColor = mix(sunsetSun, noonSun, t);

  } else if (sunHeight > -0.1) {

    float t = (sunHeight + 0.1) / 0.3;
    sunColor = mix(nightSun, sunsetSun, t);

  } else {

    sunColor = nightSun;
  }

  return sunColor;
}

float hash12(vec2 p) {
  p = fract(p * vec2(5.3983, 5.4427));
  p += dot(p.yx, p.xy + vec2(21.5351, 14.3137));
  return fract(p.x * p.y * 95.4337);
}

vec2 hash22(vec2 p) {
  p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
  return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

vec3 LayerStars(vec2 uv, float scale, float seed) {
  vec2 gridUv = uv * scale;
  vec2 gridId = floor(gridUv);
  vec2 localUv = fract(gridUv) - 0.5;

  vec3 layerColor = vec3(0.0);

  for (int y = -1; y <= 1; y++) {
    for (int x = -1; x <= 1; x++) {
      vec2 neighborOffset = vec2(float(x), float(y));
      vec2 neighborGridId = gridId + neighborOffset;
      vec2 idWithSeed = neighborGridId + vec2(seed, seed * 12.34);

      vec2 starOffset = hash22(idWithSeed) * 0.4;
      vec2 starPosInLocal = neighborOffset + starOffset;

      float dist = length(localUv - starPosInLocal);

      float brightness = 1.0 - smoothstep(0.0, 0.1, dist);
      brightness = pow(brightness, 5.0);

      float randomVal = hash12(idWithSeed);
      float twinkleSpeed = 6.0 + randomVal * 4.0;
      float timeOffset = randomVal * 6.2831;
      float twinkle = sin(u_Time * twinkleSpeed + timeOffset) * 0.35 + 0.65;
      vec3 starTint = mix(vec3(0.8, 0.9, 1.0), vec3(1.0, 0.9, 0.7), randomVal);

      layerColor += starTint * brightness * twinkle;
    }
  }
  return layerColor;
}

vec2 DirToRectilinearUV(vec3 dir) {
  vec3 absDir = abs(dir);
  float ma;
  vec2 uv;

  if (absDir.z >= absDir.x && absDir.z >= absDir.y) {
    ma = absDir.z;
    uv = dir.xy;
  } else if (absDir.y >= absDir.x) {
    ma = absDir.y;
    uv = dir.xz;
  } else {
    ma = absDir.x;
    uv = dir.yz;
  }

  uv = uv / ma;

  return uv;
}

void main() {

  vec3 V = normalize(localPos);

  vec3 L = normalize(light_position);

  vec3 sunDiskColor = getSunColor(L.y) * 1.0;

  float sunHeight = L.y;

  vec3 noonTop = vec3(0.2, 0.5, 0.9);
  vec3 noonHoriz = vec3(0.6, 0.8, 1.0);

  vec3 sunsetTop = vec3(0.2, 0.2, 0.4);
  vec3 sunsetHoriz = vec3(1.0, 0.5, 0.1);

  vec3 nightTop = vec3(0.0, 0.0, 0.1);
  vec3 nightHoriz = vec3(0.0, 0.0, 0.2);

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

  float gradient = clamp(V.y, 0.0, 1.0);

  gradient = pow(gradient, 0.5);

  float sunDot = dot(V, L);

  float sunMask = step(0.999, sunDot);
  vec3 skyColor = mix(skyHoriz, skyTop, gradient) * (1 - sunMask);

  vec3 dir = normalize(localPos);

  vec2 skyUV = DirToRectilinearUV(dir) * 2.0;
  vec3 sartColor = vec3(0.0);
  sartColor += LayerStars(skyUV, 10.0, 0.0);
  sartColor += LayerStars(skyUV, 20.0, 12.34) * 0.7;
  sartColor += LayerStars(skyUV, 40.0, 56.78) * 0.5;
  vec3 bgColor = mix(vec3(0.005, 0.01, 0.04), vec3(0.02, 0.03, 0.08),
                     smoothstep(-0.5, 0.5, dir.y));
  sartColor += bgColor;

  skyColor += sunMask * sunDiskColor;

  skyColor += sartColor * (1.0 - smoothstep(-0.2, 0, sunHeight));

  frag_color = vec4(skyColor, 1.0);
}
