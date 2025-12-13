#version 410

uniform sampler2D shadow_texture;
uniform sampler2D depth_texture;

uniform int isApplyShadow;
uniform int isVolumetricLight;
uniform mat4 light_world_to_clip_matrix;
uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_view;

uniform mat4 vertex_view_to_projection;

// 摄像机位置
uniform vec3 camera_position;

// 光源参数
uniform vec3 light_position;
uniform vec3 light_direction;

// 屏幕反向分辨率
uniform vec2 inverse_screen_resolution;

in VS_OUT { vec3 world_pos; }
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
  return sunColor;
}
float Random(vec2 co) {
  return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

float GetAdaptiveIntensity(vec3 currentPos, vec3 sunDir, vec3 rayDir) {

  // 1. 【高度衰减】基于位置 (Position Based)
  // 逻辑：树根处(0m)光很强，树冠处(15m+)光变弱。
  // 这样光束在穿过树冠时会变得柔和，不会掩盖叶子细节。
  float height = max(0.0, currentPos.y);
  // 0米处强度 1.0，20米处强度降为 0.2
  float heightAtten = 1.0 - smoothstep(0.0, 25.0, height) * 0.8;

  // 2. 【角度衰减】基于视角 (Angle Based)
  // 逻辑：当你越是正对太阳看，我就越把强度压低一点，防止过曝。
  float lookAtSun = dot(rayDir, sunDir); // 1.0 表示直视太阳
  // 如果直视太阳(>0.9)，强度乘数会变小(比如乘以0.5)
  // 侧面看时，强度保持 1.0
  float angleAtten = 1.0 - smoothstep(0.8, 1.0, lookAtSun) * 0.8;

  return heightAtten * angleAtten;
}

vec3 CalculateVolumetricFog(vec3 worldPos, vec3 cameraPos, vec3 sunDir,
                            vec3 sunHighIntensityColor, mat4 lightMatrix,
                            sampler2D shadowMap, vec2 screenPos) {

  int STEPS = 32; // 步数，越多越好
  float MAX_DISTANCE = 150.0;

  vec3 rayVector = worldPos - cameraPos;
  float rayLength = length(rayVector);
  vec3 rayDir = rayVector / rayLength;
  float targetDistance = min(rayLength, MAX_DISTANCE);
  float stepLength = targetDistance / float(STEPS);

  // Dithering 抖动
  float jitter = Random(screenPos + vec2(sin(sunDir.x), cos(sunDir.z)));
  vec3 currentPos = cameraPos + rayDir * stepLength * jitter;

  vec3 accumulatedLight = vec3(0.0);
  float VOLUME_FOG_DENSITY = 0.1;

  float sunIndensity = 1.0 - smoothstep(0.2, -0.1, sunDir.y);
  float dynamicFactor = GetAdaptiveIntensity(currentPos, sunDir, rayDir);
  sunIndensity *= dynamicFactor;
  float lightPercent = 0.0;
  float hitDistance = length(worldPos - cameraPos);

  for (int i = 0; i < STEPS; ++i) {
    vec4 clipPos = lightMatrix * vec4(currentPos, 1.0);
    vec3 projCoords = clipPos.xyz / clipPos.w;
    projCoords = projCoords * 0.5 + 0.5;
    float shadow = 1.0;
    // if (projCoords.z < 1.0 && projCoords.x > 0.0 && projCoords.x < 1.0 &&
    //     projCoords.y > 0.0 && projCoords.y < 1.0) {
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    // 假设 ShadowMap 背景是 1.0，物体深度 < 1.0
    if (projCoords.z > closestDepth + 0.005)
      shadow = 0.0; // 在阴影里
    // }

    lightPercent = mix(lightPercent, shadow, 1.0f / float(i + 1));
    currentPos += rayDir * stepLength;
  }
  float absorb = exp(-hitDistance * VOLUME_FOG_DENSITY * sunIndensity);

  return mix(vec3(0, 0, 0), sunHighIntensityColor, lightPercent) * absorb;
}
float LinearizeDepth(float depth) {
  float zNear = 0.01;          // 必须和你 C++ 设置的 projection 一致
  float zFar = 1000.0;         // 必须和你 C++ 设置的 projection 一致
  float z = depth * 2.0 - 1.0; // Back to NDC
  return (2.0 * zNear * zFar) / (zFar + zNear - z * (zFar - zNear));
}
void main() {

  vec2 texcoord = gl_FragCoord.xy * inverse_screen_resolution;
  float depth = texture(depth_texture, texcoord).r;
  float ndc_d = depth * 2.0 - 1.0;
  mat4 view_projection =
      vertex_view_to_projection * vertex_world_to_view * vertex_model_to_world;
  vec4 world_pos =
      inverse(view_projection) * vec4(texcoord.xy * 2.0 - 1.0, ndc_d, 1.0);
  world_pos /= world_pos.w;
  // depth = LinearizeDepth(depth) / 1000.0;
  //  --- 计算体积光 ---
  vec3 L = normalize(light_position);

  vec3 volumetricLight = vec3(0.0);
  float a = 1.0;
  float translucencyIntensity = 1.0;
  if (isVolumetricLight == 1) {

    vec3 sunColor = getSunColor(L.y);
    volumetricLight =
        CalculateVolumetricFog(world_pos.xyz, camera_position, L, sunColor,
                               light_world_to_clip_matrix, shadow_texture,
                               gl_FragCoord.xy) *
        a;
    // volumetricLight = pow(volumetricLight, vec3(-1.0 / 2.2));
  }

  // volumetricLight = pow(volumetricLight, vec3(1.0 / 1.2));
  frag_color = vec4(vec3(volumetricLight), 1.0);
}
