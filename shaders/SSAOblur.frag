#version 410
out vec4 FragColor;
uniform vec2 inverse_screen_resolution;
uniform sampler2D ssaoInput;
in vec2 TexCoords;

void main() {
  vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
  float result = 0.0;
  for (int x = -2; x < 2; ++x) {
    for (int y = -2; y < 2; ++y) {
      vec2 offset = vec2(float(x), float(y)) * texelSize;
      result += texture(ssaoInput, TexCoords + offset).r;
    }
  }

  //   FragColor = vec4(vec3(texture(ssaoInput, TexCoords).r), 1.0);
  FragColor = vec4(result) / (4.0 * 4.0);
}