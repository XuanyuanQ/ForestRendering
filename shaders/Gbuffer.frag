#version 410
uniform sampler2D txture_alpha;
uniform sampler2D txture;
uniform sampler2D normals_texture;
uniform int lables; // 0-terrain,1-leaves,2-bark,3-grass

in VS_OUT {
  vec3 normal;
  vec2 texcoord;
  vec3 tangent;
  vec3 binormal;
  mat3 TBN; // 输出 TBN
  mat4 normal_model_to_world;
}
fs_in;

layout(location = 0) out vec4 geometry_diffuse;
layout(location = 1) out vec4 geometry_specular;
layout(location = 2) out vec4 geometry_normal;
layout(location = 3) out
    float objectType; // 0 - terrain, 1 - leaves, 2 - bark,3 - grass

void main() {
  objectType = float(lables);
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

  geometry_diffuse = texture(txture, fs_in.texcoord);

  geometry_specular = texture(txture, fs_in.texcoord);
  //   geometry_diffuse = vec4(0.1, 0.5, 0.0, 1.0);

  vec3 normal =
      texture(normals_texture, fs_in.texcoord).rgb; // normal in tagen space
  vec3 realNormal = normalize(
      fs_in.TBN *
      (2.0 * normal - 1.0)); // transform norma form tagent space to model space
  vec4 worldNormal =
      fs_in.normal_model_to_world *
      vec4(normalize(realNormal), 0.0); // form model space to world space
  geometry_normal.xyz = (normalize(worldNormal.xyz) * 0.5 + 0.5);
  geometry_normal.w = 1.0;
}
