#version 410

uniform sampler2D diffuse_texture;
uniform int has_diffuse_texture;

uniform sampler2D leaves_alpha;
uniform sampler2D maple_bark;
uniform sampler2D maple_leaf;
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
  if (has_diffuse_texture != 0) {
    if (is_leaves != 0) {
      vec4 leavesMask = texture(leaves_alpha, fs_in.texcoord);
      if (leavesMask == vec4(1.0)) {
        discard;
      } else {
        frag_color =
            (kd + ks) * vec4(texture(diffuse_texture, fs_in.texcoord).rgb, 1.0);
      }

    } else {
      frag_color = (kd + ks) * texture(diffuse_texture, fs_in.texcoord);
    }

  } else {
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
  // frag_color = (kd + ks) * texture(diffuse_texture, fs_in.texcoord);
}
