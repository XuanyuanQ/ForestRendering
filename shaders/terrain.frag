#version 410
uniform sampler2D grass_texture;
in float Height;
in vec2 tess_texcoord_fs;

out vec4 frag_color;

void main() {
  float h = (Height + 4) / 8.0f;
  vec3 soilColor = vec3(0.55, 0.27, 0.07);
  frag_color = vec4(soilColor * h, h);
  // frag_color = vec4(h, h, h, 1.0);
  // frag_color = vec4(1.0);
}
