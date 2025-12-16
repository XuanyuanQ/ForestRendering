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

  float grey = dot(color, vec3(0.299, 0.587, 0.114));
  return mix(vec3(grey), color, saturation);
}

vec3 adjustLeavesCol(vec3 stayGreen, vec3 turnOrange, vec3 turnRed) {

  float noise = 1.2 * random(float(v_InstanceID));

  float t = pow(noise, 3.5);

  vec3 finalTint;

  if (t < 0.5) {

    float subT = t * 2.0;
    finalTint = mix(stayGreen, turnOrange, subT);
  } else {

    float subT = (t - 0.5) * 2.0;
    finalTint = mix(turnOrange, turnRed, subT);
  }

  float brightness = 1.0 - t * 0.2; // 0.8 ~ 1.0
  return finalTint * brightness;
}
void calculateTrees(in float shininess, in float specularStrength,
                    in vec4 albedoTexture, in vec3 L, in vec3 V, in vec3 N,
                    out vec3 ambient, out vec3 diffuse, out vec3 specular) {

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

  ambient = skyAmbient * albedoTexture.rgb;

  float diff = max(dot(L, N), 0.0);
  //   diff = 1.0;
  //   diffuse = diff * sunColor * albedoTexture.rgb;
  diffuse = diff * sunColor * albedoTexture.rgb;

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
