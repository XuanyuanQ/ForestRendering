#version 410

in vec2 TexCoords;
in vec4 ParticleColor;

uniform sampler2D sprite;

out vec4 frag_color;

void main() {
  // frag_color = (texture(sprite, TexCoords) * ParticleColor);
  frag_color = vec4(1.0, 0.8f, 0.5f, 1.0f) * ParticleColor;
}
