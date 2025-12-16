#version 410

layout(std140) uniform Shadows {
  mat4 lightSpaceMatrices[16];
  float cascadePlaneDistances[16];
  vec4 lightDir;
  int cascadeCount;
};

uniform sampler2D txture_alpha;
uniform sampler2D txture;
uniform sampler2D normals_texture;
uniform sampler2D shadow_texture;
uniform sampler2DArray shadow_Array;

uniform int lables; // 0-terrain,1-leaves,2-bark,3-grass
uniform int isApplyShadow;
uniform int isVolumetricLight;
uniform int applySSAO;
uniform mat4 light_world_to_clip_matrix;
uniform mat4 vertex_world_to_view;

uniform vec3 camera_position;

uniform vec3 light_position;
uniform vec3 light_direction;

// Screen reverse resolution
uniform vec2 inverse_screen_resolution;
uniform sampler2D ssaoBlur;

in VS_OUT {
  vec3 normal;
  vec2 texcoord;
  vec3 tangent;
  vec3 binormal;
  mat3 TBN;
  vec4 ndc;
  vec3 world_pos;
  mat4 normal_model_to_world;
  vec3 fV;
  vec3 fL;
  vec4 FragPosLightSpace;
}
fs_in;
flat in int v_InstanceID;
flat in int v_VertexID;
out vec4 frag_color;

vec3 ACESFilm(vec3 x) {
  float a = 2.51f;
  float b = 0.03f;
  float c = 2.43f;
  float d = 0.59f;
  float e = 0.14f;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}
vec3 getSunColor(float sunHeight) {
  vec3 noonSun = vec3(1.0, 0.98, 0.9);
  vec3 sunsetSun = vec3(1.0, 0.4, 0.1);
  vec3 nightSun = vec3(0.0, 0.0, 0.0);

  vec3 sunColor;
  if (sunHeight > 0.2) {
    // Daytime -> Midday (a blend of sunset and midday colors)
    float t = (sunHeight - 0.2) / 0.8;
    sunColor = mix(sunsetSun, noonSun, t);

  } else if (sunHeight > -0.1) {
    // Sunset -> Evening (a blend of evening and sunset colors)
    float t = (sunHeight + 0.1) / 0.3;
    sunColor = mix(nightSun, sunsetSun, t);

  } else {
    // Night
    sunColor = nightSun;
  }
  float factor = pow(150, sunHeight * 0.6 + 1.0);
  return sunColor;
}

void calculateGrass(in vec4 albedoTexture, in vec3 L, in vec3 V, in vec3 N,
                    out vec3 ambient, out vec3 diffuse, out vec3 specular) {
  // -----------------------------------------------------------
  // 3. Dynamic sky color
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
  // 4. Lighting calculation
  // -----------------------------------------------------------

  // Ambient light
  ambient = skyAmbient * albedoTexture.rgb * 0.5;

  // Diffuse reflection
  float diff = max(dot(L, N), 0.0);
  diffuse = diff * sunColor * albedoTexture.rgb;

  // Highlights
  float spec = pow(max(dot(V, R), 0.0), 10.0);
  specular = spec * sunColor * 0.1;

  if (sunHeight > -0.1) {
    diffuse = diffuse * 0.3;
    specular = vec3(0.0, 0.38, 0.0) * 0.1;
  }
}

vec3 calculateTrans(in vec4 albedoTexture, in vec3 L, in vec3 V, in vec3 N) {
  float distortion = 0.1;
  float power = 4.0;
  float scale = 0.1;
  vec3 sunCol = getSunColor(L.y);
  vec3 distortedLightVector = normalize(-L + N * distortion);

  float translucencyDot = max(0.0, dot(V, distortedLightVector));

  // 3. Apply focus and intensity
  float translucencyIntensity = pow(translucencyDot, power) * scale;
  return translucencyIntensity * sunCol;
}
// --------------------------------------
// Terrain lighting calculation
// --------------------------------------
void calculateTrees(in float shininess, in float specularStrength,
                    in vec4 albedoTexture, in vec3 L, in vec3 V, in vec3 N,
                    out vec3 ambient, out vec3 diffuse, out vec3 specular) {

  // 1. Obtain the sun's altitude (0.0 is the horizon, 1.0 is the zenith)
  // We use L.y (the vertical component of the illumination vector) to determine
  // this.
  float sunHeight = L.y;
  vec3 R = reflect(-L, N);

  // 2. Define the color of sunlight at different times (Light Color)
  vec3 noonSun = vec3(1.0, 0.98, 0.9);  // Noon: Warm White
  vec3 sunsetSun = vec3(1.0, 0.4, 0.1); // Sunset: Orange-red
  vec3 nightSun = vec3(0.0, 0.0, 0.0);  // Night: No light

  // 3. Define the ambient color at different times.
  vec3 noonAmb = vec3(0.4, 0.4, 0.45); // Midday environment: bright blue-gray
  vec3 sunsetAmb =
      vec3(0.3, 0.2, 0.2); // Sunset environment: dark reddish-brown
  vec3 nightAmb =
      vec3(0.02, 0.02, 0.05); // Evening environment: dark blue-black

  // 4.Based on highly mixed colors
  vec3 sunColor;
  vec3 skyAmbient;

  if (sunHeight > 0.2) {
    // Daytime -> Midday (a blend of sunset and midday colors)
    float t = (sunHeight - 0.2) / 0.8;
    sunColor = mix(sunsetSun, noonSun, t);
    skyAmbient = mix(sunsetAmb, noonAmb, t);
  } else if (sunHeight > -0.1) {
    // Sunset -> Evening (a blend of evening and sunset colors)
    float t = (sunHeight + 0.1) / 0.3;
    sunColor = mix(nightSun, sunsetSun, t);
    skyAmbient = mix(nightAmb, sunsetAmb, t);
  } else {
    // Night
    sunColor = nightSun;
    skyAmbient = nightAmb;
  }

  // -------------------------------------------------------------
  // 5. Apply lighting (using dynamically calculated sunColor and skyAmbient).
  // -------------------------------------------------------------

  // Ambient light = Dynamic ambient color * Material's inherent color
  ambient = skyAmbient * albedoTexture.rgb;

  // Diffuse reflection = Diffuse reflection intensity * Dynamic sunlight color
  // * Material inherent color
  float diff = max(dot(L, N), 0.0);
  //   diff = 1.0;
  //   diffuse = diff * sunColor * albedoTexture.rgb;
  diffuse = diff * sunColor * albedoTexture.rgb;

  // Highlight = Highlight Intensity * Dynamic Sunlight Color
  float spec = pow(max(dot(V, R), 0.0), shininess);
  specular = spec * specularStrength * sunColor;
}

// --------------------------------------
// Tree light calculation
// --------------------------------------
void calculateTerrain(in vec4 albedoTexture, in vec3 L, in vec3 V, in vec3 N,
                      out vec3 ambient, out vec3 diffuse, out vec3 specular) {
  // -----------------------------------------------------------
  // 3. Dynamic sky color
  // -----------------------------------------------------------
  // Determine the solar altitude using the Y component of the illumination
  // vector
  float sunHeight = L.y;
  vec3 R = reflect(-L, N);

  // Define the sky color (noon, sunset, night).
  vec3 noonSun = vec3(1.0, 0.98, 0.9);
  vec3 sunsetSun = vec3(1.0, 0.4, 0.1);
  vec3 nightSun = vec3(0.0, 0.0, 0.0);

  // Define ambient light color
  vec3 noonAmb = vec3(0.4, 0.4, 0.45);
  vec3 sunsetAmb = vec3(0.3, 0.2, 0.2);
  vec3 nightAmb = vec3(0.02, 0.02, 0.05);

  vec3 sunColor;
  vec3 skyAmbient;

  // Mixed logic
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
  // 4. Lighting calculation
  // -----------------------------------------------------------

  ambient = skyAmbient * albedoTexture.rgb;

  float diff = max(dot(L, N), 0.0);
  diffuse = diff * sunColor * albedoTexture.rgb * 0.3;

  float spec = pow(max(dot(V, R), 0.0), 50.0);
  specular = spec * sunColor * 0.1;

  if (sunHeight > -0.1) {
    ambient = ambient * 0.4;
    diffuse = diffuse * 0.3;
    specular = vec3(0.5, 0.38, 0.2) * 0.1;
  }
}

int CalculateCascadeLayer(vec3 fragPosWorld, mat4 viewMatrix) {
  vec4 fragPosViewSpace = viewMatrix * vec4(fragPosWorld, 1.0);
  float depthValue = abs(fragPosViewSpace.z);

  int layer = -1;
  for (int i = 0; i < cascadeCount; ++i) {
    if (depthValue < cascadePlaneDistances[i]) {
      layer = i;
      break;
    }
  }

  if (layer == -1) {
    layer = cascadeCount;
  }

  return layer;
}

float ShadowCalculation(vec3 fragPosWorld, vec3 normal, vec3 L,
                        vec2 shadowmap_texel_size, int ispcf) {

  int layer = CalculateCascadeLayer(fragPosWorld, vertex_world_to_view);

  vec4 fragPosLightSpace = lightSpaceMatrices[layer] * vec4(fragPosWorld, 1.0);

  vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

  projCoords = projCoords * 0.5 + 0.5;

  if (projCoords.z > 1.0)
    return 0.0;

  float bias = max(0.005 * (1.0 - dot(normal, L)), 0.0005);
  if (layer == cascadeCount)
    bias *= 0.5;
  float currentDepth = projCoords.z;

  float shadow = 0.0;
  if (ispcf == 0) {
    float pcfDepth = texture(shadow_Array, vec3(projCoords.xy, float(layer))).r;
    shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
  } else {
    for (int x = -1; x <= 1; ++x) {
      for (int y = -1; y <= 1; ++y) {

        float pcfDepth =
            texture(shadow_Array,
                    vec3(projCoords.xy + vec2(x, y) * shadowmap_texel_size,
                         float(layer)))
                .r;
        shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
      }
    }
    shadow /= 9.0;
  }

  return 1.0 - shadow;
}

float calculateLight(vec3 world_pos, mat4 light_projection,
                     vec2 shadowmap_texel_size) {
  vec4 clip_pos = light_projection * vec4(world_pos, 1.0);

  // 1. Calculate NDC coordinates
  vec3 projCoords = clip_pos.xyz / clip_pos.w;

  // 2. Transform to the [0, 1] interval (for texture sampling and depth
  // comparison)
  projCoords = projCoords * 0.5 + 0.5;

  // 3. Solving the problem of extending beyond the view frustum boundary
  if (projCoords.z > 1.0)
    return 1.0;

  float current_depth = projCoords.z;

  // 4. Calculate Bias
  float bias = 0.005 / clip_pos.w;

  float shadow_sum = 0.0;

  // PCF 5x5
  for (int i = -2; i <= 2; ++i) {
    for (int j = -2; j <= 2; ++j) {
      // Sample the ShadowMap (values ​​between 0.0 and 1.0).
      float closest_depth =
          texture(shadow_texture,
                  projCoords.xy + vec2(i, j) * shadowmap_texel_size)
              .r;

      // Comparison logic:
      // If "current depth - bias" > "recent depth", then I am behind -> Shadow
      // (1.0) Otherwise -> Highlight (0.0)
      if (current_depth - bias > closest_depth)
        shadow_sum += 1.0;
    }
  }

  // Calculate the average shadow value
  // shadow_sum represents the number of points that identify as shadow.
  // If all 25 points identify as shadow, then shadow_factor = 1.0
  float shadow_factor = shadow_sum / 25.0;
  return 1.0 - shadow_factor;
  //   shadow_sum = texture(shadow_texture, projCoords.xy).r;
  //   return shadow_sum;
}

// ==================================================================================
// Volumetric light computation core function
// ==================================================================================

float Random(vec2 co) {
  return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}
float random(float seed) { return fract(sin(seed) * 43758.5453123); }
vec3 adjustSaturation(vec3 color, float saturation) {
  // Calculate grayscale values ​​(brightness).
  float grey = dot(color, vec3(0.299, 0.587, 0.114));
  // Interpolation between grayscale and primary colors
  return mix(vec3(grey), color, saturation);
}
vec3 adjustLeavesCol(vec3 stayGreen, vec3 turnOrange, vec3 turnRed) {

	float rawCos = cos(float(v_InstanceID));
	float noise = 1.6 * random(float(v_VertexID)) * (rawCos * 0.5 + 0.5);
	
  // pow(noise, 2.5) means that most results will be relatively small (leaning
  // towards green), Only when the noise approaches 1.0 will the results rapidly
  // increase (turn red). This matches the feeling of "early autumn": mostly
  // green/yellow, with a small portion of red.
  float t = pow(noise, 3.5);

  vec3 finalTint;

  // We use two mixes to implement the three-stage transition
  if (t < 0.5) {
    // First 50% probability: Transition between "dark green" and "golden
    // orange" Map t from [0.0, 0.5] to [0.0, 1.0]
    float subT = t * 2.0;
    finalTint = mix(stayGreen, turnOrange, subT);
  } else {
    // The last 50% probability: transitioning between "Golden Orange" and
    // "Fiery Red" Map t from [0.5, 1.0] to [0.0, 1.0]
    float subT = (t - 0.5) * 2.0;
    finalTint = mix(turnOrange, turnRed, subT);
  }

  // --- 【Brightness Adjustment】 ---
  // Red leaves usually appear slightly darker than green leaves
  // If t is relatively high (leaning towards red), slightly reduce the
  // brightness.
  float brightness = 1.0 - t * 0.2; // 0.8 ~ 1.0
  return finalTint * brightness;
}

// Mie scattering phase function
float GetMiePhase(float g, float cosTheta) {
  // Numerical stability optimization for extremely high G values
  float g2 = g * g;
  float num = 1.0 - g2;
  float denom = 1.0 + g2 - 2.0 * g * cosTheta;
  // To prevent the denominator from being 0
  denom = max(denom, 0.0001);
  return num / (4.0 * 3.14159265 * pow(denom, 1.5));
}

float GetAdaptiveIntensity(vec3 currentPos, vec3 sunDir, vec3 rayDir) {

  // 1. **[Height Attenuation]** Based on Position
  // Logic: Light is very strong at the tree roots (0m), and weaker at the tree
  // crown (15m+). This softens the light beam as it passes through the crown,
  // preventing it from obscuring leaf details.
  float height = max(0.0, currentPos.y);
  // Strength at 0 meters is 1.0, and strength at 20 meters decreases to 0.2.
  float heightAtten = 1.0 - smoothstep(0.0, 25.0, height) * 0.8;

  // 2. [Angle Attenuation] Based on Viewpoint
  // Logic: The closer you are to the sun, the lower the intensity will be to
  // prevent overexposure.
  float lookAtSun =
      dot(rayDir, sunDir); // 1.0 indicates looking directly at the sun

  // If you are looking directly at the sun (>0.9), the intensity multiplier
  // will decrease (e.g., multiply by 0.5). When viewed from the side, the
  // intensity remains at 1.0.
  float angleAtten = 1.0 - smoothstep(0.8, 1.0, lookAtSun) * 0.8;

  return heightAtten * angleAtten;
}

vec3 CalculateVolumetricFog(vec3 worldPos, vec3 cameraPos, vec3 sunDir,
                            vec3 sunHighIntensityColor, mat4 lightMatrix,
                            sampler2D shadowMap, vec2 screenPos, vec3 N,
                            vec2 shadowmap_texel_size) {

  int STEPS = 32;
  float MAX_DISTANCE = 250.0;

  vec3 rayVector = worldPos - cameraPos;
  float rayLength = length(rayVector);
  vec3 rayDir = rayVector / rayLength;
  float targetDistance = min(rayLength, MAX_DISTANCE);
  targetDistance = rayLength;
  float stepLength = targetDistance / float(STEPS);

  // Dithering
  float jitter = Random(screenPos + vec2(sin(sunDir.x), cos(sunDir.z)));
  vec3 currentPos = cameraPos + rayDir * stepLength * jitter;

  vec3 accumulatedLight = vec3(0.0);
  float VOLUME_FOG_DENSITY = 0.01;

  float sunIndensity = 1.0 - smoothstep(0.2, -0.1, sunDir.y);
  float dynamicFactor = GetAdaptiveIntensity(currentPos, sunDir, rayDir);
  sunIndensity *= dynamicFactor;
  float scatter = GetMiePhase(0.8, dot(normalize(sunDir), normalize(rayDir)));
  // scatter = 1.0;
  float lightPercent = 0.0;
  float hitDistance = length(worldPos - cameraPos);
  for (int i = 0; i < STEPS; ++i) {
    // float shadow = ShadowCalculation(
    //     currentPos, normalize(N), normalize(sunDir), shadowmap_texel_size,
    //     0);
    vec4 clipPos = lightMatrix * vec4(currentPos, 1.0);
    vec3 projCoords = clipPos.xyz / clipPos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    float shadow = 1.0;
    // if (projCoords.z < 1.0 && projCoords.x > 0.0 && projCoords.x < 1.0 &&
    //     projCoords.y > 0.0 && projCoords.y < 1.0) {
    float closestDepth = texture(shadowMap, projCoords.xy).r * 2.0 - 1.0;
    // Assume the ShadowMap background is 1.0 and the object depth is < 1.0.
    if (projCoords.z > closestDepth + 0.0005) {
      shadow = 0.0; // In the shadows
    }
    // }

    lightPercent = mix(lightPercent, shadow * scatter, 1.0f / float(i + 1));
    // lightPercent += shadow;
    currentPos += rayDir * stepLength;
  }

  float absorb = exp(-hitDistance * VOLUME_FOG_DENSITY * sunIndensity);
  // return vec3(shadow);
  return mix(vec3(0, 0, 0), sunHighIntensityColor, min(5, lightPercent)) *
         absorb;
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

  vec3 finalNormal;
  if (lables == 1) {
    vec3 rawNormal = texture(normals_texture, fs_in.texcoord).rgb;

    rawNormal = rawNormal * 2.0 - 1.0;
    rawNormal.xy *= 0.5;
    finalNormal = normalize(fs_in.TBN * rawNormal);

    shininess = 10.0;
    specularStrength = 0.1;

    vec3 stayGreen = vec3(0.6, 0.8, 0.8);
    vec3 turnOrange = vec3(0.8, 0.6, 0.3);
    vec3 turnRed = vec3(1.6, 0.3, 0.2);

    albedoTexture.rgb *= adjustLeavesCol(stayGreen, turnOrange, turnRed);
    albedoTexture.rgb = adjustSaturation(albedoTexture.rgb, 1.1);
  } else if (lables == 2) {
    vec3 rawNormal = texture(normals_texture, fs_in.texcoord).rgb;
    float noise = 1.0 * random(float(v_InstanceID));
    rawNormal = rawNormal * 2.0 - 1.0;
    rawNormal.xy *= 1.2 * noise;
    finalNormal = normalize(fs_in.TBN * rawNormal);
    shininess = 5.0 * noise;
    specularStrength = 0.02 * noise;
    vec3 woodTint = vec3(1.0, 0.8, 0.6) * (noise + .5);
    albedoTexture.rgb *= woodTint;

  } else {
    if (lables == 0) {
      albedoTexture = texture(txture, fs_in.texcoord * 20.0);
    }
    finalNormal = normalize(fs_in.normal);
  }
  //   finalNormal = normalize(fs_in.normal);
  vec3 diffuse, specular, ambient;
  diffuse = vec3(0.0);
  specular = vec3(0.0);
  ambient = vec3(0.0);
  vec3 trans = vec3(0.0);
  if (lables == 3) {
    vec3 stayGreen = vec3(0.9, 0.95, 0.6);
    vec3 turnStraw = vec3(0.5, 0.4, 0.9);
    vec3 turnBrown = vec3(0.7, 0.6, 0.4);
    albedoTexture.rgb *= adjustLeavesCol(stayGreen, turnStraw, turnBrown);
    calculateGrass(albedoTexture, L, V, finalNormal, ambient, diffuse,
                   specular);
  } else if (lables == 1 || lables == 2) {
    calculateTrees(shininess, specularStrength, albedoTexture, L, V,
                   finalNormal, ambient, diffuse, specular);

  } else {
    calculateTerrain(albedoTexture, L, V, finalNormal, ambient, diffuse,
                     specular);
  }

  vec2 shadowmap_texel_size = 1.0 / textureSize(shadow_texture, 0);
  float light_pcf = calculateLight(
      fs_in.world_pos.xyz, light_world_to_clip_matrix, shadowmap_texel_size);
  // float light_pcf = ShadowCalculation(fs_in.world_pos.xyz, finalNormal, L,
  //                                     shadowmap_texel_size, 1);
  if (0 == isApplyShadow) {
    light_pcf = 1.0;
  }
  // float translucencyIntensity = 1.0;
  vec3 translucencyIntensity = calculateTrans(albedoTexture, L, V, finalNormal);

  float AmbientOcclusion = 1.0;
  if (applySSAO == 1) {
    ivec2 ssaoSize = textureSize(ssaoBlur, 0);
    vec2 ssaoUV = gl_FragCoord.xy / ssaoSize;
    AmbientOcclusion = texture(ssaoBlur, ssaoUV).r;
    AmbientOcclusion = mix(1.0, AmbientOcclusion, 0.3);
  }
  vec3 sceneColor = light_pcf * (diffuse + specular + translucencyIntensity) +
                    ambient * AmbientOcclusion;

  // --- Calculate volumetric light ---

  vec3 volumetricLight = vec3(0.0);

  if (isVolumetricLight == 1) {
    vec3 sunColor = getSunColor(L.y);
    volumetricLight = CalculateVolumetricFog(
        fs_in.world_pos, camera_position, L, sunColor,
        light_world_to_clip_matrix, shadow_texture, gl_FragCoord.xy,
        finalNormal, shadowmap_texel_size);
  }
  float diff = max(dot(L, finalNormal), 0.0);
  vec3 R = reflect(-L, finalNormal);
  float spec = pow(max(dot(V, R), 0.0), 50.0);
  // vec3 final_color = sceneColor + volumetricLight * (3 * sceneColor);
  vec3 final_color = sceneColor + volumetricLight * sceneColor * 1;
  // final_color = final_color / (final_color + vec3(1.0));
  final_color = ACESFilm(final_color);
  // final_color = pow(final_color, vec3(1.0 / 1.2));

  if (isApplyShadow == 1 && isVolumetricLight == 1) {
    frag_color = vec4(final_color, 1.0);
  } else if (isVolumetricLight == 1) {
    frag_color = vec4(volumetricLight, 1.0);
  } else {
    frag_color = vec4(vec3(sceneColor), 1.0);
  }
}
