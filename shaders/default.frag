#version 410

uniform sampler2D diffuse_texture;
uniform int has_diffuse_texture;
uniform int has_normal_texture;

uniform sampler2D leaves_alpha;
uniform sampler2D maple_bark;
uniform sampler2D maple_leaf;
uniform int is_leaves;
uniform sampler2D maple_leaf_normal;
uniform sampler2D maple_bark_normal;

in VS_OUT {
  vec3 normal;
  vec2 texcoord;
  vec3 fV;
  vec3 fL;
  mat3 TBN;
}
fs_in;

out vec4 frag_color;

void main() {

  vec3 N;
  vec3 normalMapValue;
  if (is_leaves != 0) {
    // --- 叶子 ---
    normalMapValue = texture(maple_leaf_normal, fs_in.texcoord).rgb;

  } else {
    // --- 树干 ---
    normalMapValue = texture(maple_bark_normal, fs_in.texcoord).rgb;
  }

  normalMapValue = normalMapValue * 2.0 - 1.0; // [0,1] -> [-1,1]
  N = normalize(fs_in.TBN * normalMapValue);   // 转到世界空间

  N = normalize(fs_in.normal);
  float kd = max(0.0, dot(normalize(fs_in.fL), N));
  vec3 R = reflect(normalize(-fs_in.fL), N);
  float ks = max(0.0, pow(dot(normalize(fs_in.fV), normalize(R)), 50.0));

  if (is_leaves != 0) {
    vec4 leavesMask = texture(leaves_alpha, fs_in.texcoord);
    if (leavesMask != vec4(1.0)) {
      discard;
    } else {
      frag_color = (kd)*vec4(texture(maple_leaf, fs_in.texcoord).rgb, 1.0);
    }
  } else {
    frag_color = (kd)*texture(maple_bark, fs_in.texcoord);
  }
  // frag_color = vec4(1.0);
}
