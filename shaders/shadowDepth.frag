#version 410
uniform sampler2D txture_alpha;
uniform int isGbufferDepth;
uniform int lables; // 0-terrain,1-leaves,2-bark,3-grass

layout(location = 0) out vec3 gPosition;
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec3 gAlbedo;

in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;

in VS_OUT { vec2 texcoord; }
fs_in;
out vec4 frag_color;
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
  if (isGbufferDepth == 1) {
    gPosition = FragPos;
    gNormal = normalize(Normal);
    gAlbedo.rgb = vec3(0.95);
    FragColor = vec4(FragPos, 1.0);
  }
}
