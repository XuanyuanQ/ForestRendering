#version 410 core

uniform mat4 vertex_view_to_projection;
uniform mat4 vertex_model_to_world;
uniform mat4 vertex_world_to_view;
uniform mat4 light_world_to_clip_matrix;

uniform sampler2D depth_texture;
uniform sampler2D normal_texture;
uniform sampler2D shadow_texture;
uniform isampler2D object_type;

// 屏幕反向分辨率
uniform vec2 inverse_screen_resolution;

// 摄像机位置
uniform vec3 camera_position;

// 光源参数
uniform vec3 light_position;
uniform vec3 light_direction;
// out vec4 frag_color;
// 输出
layout(location = 0) out vec4 light_diffuse_contribution;
layout(location = 1) out vec4 light_specular_contribution;
layout(location = 2) out vec4 light_ambient_contribution;

// --------------------------------------
// 草地光照计算
// --------------------------------------
void calculateGrass(in vec3 L, in vec3 V, in vec3 N, out vec3 ambient,
                    out vec3 diffuse, out vec3 specular) {
  float sunHeight = L.y;

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

  // 环境光
  ambient = skyAmbient * 0.5;

  // 漫反射
  float diff = max(dot(L, N), 0.0);
  diffuse = diff * sunColor;

  // 高光
  vec3 R = reflect(-normalize(L), normalize(N));
  float spec = pow(max(dot(V, R), 0.0), 10.0);
  specular = spec * sunColor * 0.1;
}

// --------------------------------------
// 地形光照计算
// --------------------------------------
void calculateTerrain(in vec3 L, in vec3 V, in vec3 N, out vec3 ambient,
                      out vec3 diffuse, out vec3 specular) {
  float sunHeight = L.y;

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

  ambient = skyAmbient;

  float diff = max(dot(L, N), 0.0);
  diffuse = diff * sunColor;

  vec3 R = reflect(-normalize(L), normalize(N));
  float spec = pow(max(dot(V, R), 0.0), 5.0);
  specular = spec * sunColor * 0.1;
}

// --------------------------------------
// 树木光照计算
// --------------------------------------
void calculateTrees(in int isLeaves, in vec3 L, in vec3 V, in vec3 N,
                    out vec3 ambient, out vec3 diffuse, out vec3 specular) {
  float shininess = 30.0;
  float specularStrength = 0.3;
  vec3 woodTint = vec3(1.0, 1.0, 1.0);

  if (isLeaves == 1) {
    woodTint = vec3(1.0, 0.8, 0.6);
    shininess = 5.0;
    specularStrength = 0.02;
  }

  float sunHeight = L.y;

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

  ambient = skyAmbient;

  float diff = max(dot(L, N), 0.0);
  diffuse = diff * sunColor;

  vec3 R = reflect(-normalize(L), normalize(N));
  float spec = pow(max(dot(V, R), 0.0), shininess);
  specular = spec * specularStrength * sunColor;
}

// --------------------------------------
// 主函数
// --------------------------------------
void main() {
  float light_intensity = 20.0 * 100.0 * 100.0;
  float light_angle_falloff = radians(37.0);
  vec2 shadowmap_texel_size = 1.0 / textureSize(shadow_texture, 0);

  vec2 texcoord = gl_FragCoord.xy * inverse_screen_resolution;
  vec3 normal = texture(normal_texture, texcoord).rgb * 2.0 - 1.0;
  float depth = texture(depth_texture, texcoord).r;
  float ndc_d = depth * 2.0 - 1.0;

  // 这里 model_to_world 需要你传入
  mat4 view_projection =
      vertex_view_to_projection * vertex_world_to_view * vertex_model_to_world;
  vec4 world_pos =
      inverse(view_projection) * vec4(texcoord.xy * 2.0 - 1.0, ndc_d, 1.0);
  world_pos /= world_pos.w;

  vec3 L = light_position;
  vec3 V = camera_position - world_pos.xyz;

  float distance_falloff = 1.0 / max(0.0001, length(L) * length(L));

  float cosTheta = dot(normalize(light_direction), -normalize(L));
  float cutoff = cos(radians(40.0));
  float angle_falloff = pow(smoothstep(cutoff, 1.0, cosTheta), 2.0);

  // Shadow mapping PCF
  vec4 clip_pos = light_world_to_clip_matrix * world_pos;
  vec4 ndc_pos = clip_pos / clip_pos.w;
  vec2 light_uv = ndc_pos.xy * 0.5 + 0.5;
  float current_depth =
      clamp(-0.5, 0.5, ndc_pos.z * 0.5 + 0.5 - 0.0005 / clip_pos.w);

  float light_d = 0.0;
  for (int i = -3; i <= 3; ++i) {
    for (int j = -3; j <= 3; ++j) {
      vec2 offset = vec2(i, j) * shadowmap_texel_size;
      float d = texture(shadow_texture, light_uv + offset).r;
      if (d < current_depth)
        light_d += 1.0;
    }
  }
  float lightpcf = light_d / 49.0;

  ivec2 texSize = textureSize(object_type, 0);
  ivec2 texelCoord = ivec2(texcoord * texSize);
  int type = texelFetch(object_type, texelCoord, 0).r;

  vec3 diffuse, specular, ambient;
  diffuse = vec3(1.0);
  specular = vec3(1.0);
  ambient = vec3(1.0);
  if (type == 3) {
    calculateGrass(L, V, normal, ambient, diffuse, specular);
  } else if (type == 1 || type == 2) {
    calculateTrees(type, L, V, normal, ambient, diffuse, specular);
  } else {
    calculateTerrain(L, V, normal, ambient, diffuse, specular);
  }
  // float lighFactor = lightpcf * angle_falloff * distance_falloff *
  //                    light_angle_falloff * light_intensity;
  float lighFactor = lightpcf;

  light_diffuse_contribution = vec4(diffuse * lighFactor, 1.0);
  // light_diffuse_contribution = vec4(1.0);
  light_specular_contribution = vec4(specular * lighFactor, 1.0);
  light_ambient_contribution = vec4(ambient * lighFactor, 1.0);

  light_specular_contribution = vec4(vec3(ndc_pos.xyz) * 0.5 + 0.5, 1.0);
  // light_diffuse_contribution = vec4(1.0);
  // light_ambient_contribution = vec4(texture(normal_texture,
  // texcoord).rgb, 1.0); frag_color = vec4(1.0);
}
