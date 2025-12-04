#version 410

uniform sampler2D grass_alpha;
uniform sampler2D grass_texture;
uniform int is_leaves;

in VS_OUT {
  vec3 normal;
  vec2 texcoord;
  vec3 fV;
  vec3 fL;
}
fs_in;

out vec4 frag_color;

void main() {
  float kd = max(0.0, dot(normalize(fs_in.fL), normalize(fs_in.normal)));
  vec3 R = reflect(normalize(-fs_in.fL), normalize(fs_in.normal));
  float ks = max(0.0, pow(dot(normalize(fs_in.fV), normalize(R)), 50.0));
  vec4 leavesMask = texture(grass_alpha, fs_in.texcoord);
  float luminance =
      0.2126 * leavesMask.r + 0.7152 * leavesMask.g + 0.0722 * leavesMask.b;
  float threshold = 0.2;

  if (luminance < threshold) {
    discard;
  } else {
    frag_color = (kd)*vec4(texture(grass_texture, fs_in.texcoord).rgb, 1.0);
  }

  // frag_color = vec4(1.0);
}
