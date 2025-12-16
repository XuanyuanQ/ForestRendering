#version 410
out vec4 FragColor;

uniform vec2 inverse_screen_resolution;
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D noiseTexture;
in vec2 TexCoords;
// uniform vec3 samples[64];
layout(std140) uniform SSAOBlock {
  vec4 samples[64];
  int kernelSize1;
};

// parameters (you'd probably want to use them as uniforms to more easily tweak
// the effect)
int kernelSize = 32;
float radius = 0.15;
float bias = 0.0025;

// tile noise texture over screen based on screen dimensions divided by noise
// size
const vec2 noiseScale = vec2(2560.0 / 4.0, 1600.0 / 4.0);

uniform mat4 vertex_view_to_projection;

void main() {
  // get input for SSAO algorithm

  vec3 fragPos = texture(gPosition, TexCoords).xyz;
  vec3 normal = normalize(texture(gNormal, TexCoords).rgb);
  vec3 randomVec = normalize(texture(noiseTexture, TexCoords * noiseScale).xyz);
  // create TBN change-of-basis matrix: from tangent-space to view-space
  vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
  vec3 bitangent = cross(normal, tangent);
  mat3 TBN = mat3(tangent, bitangent, normal);
  // iterate over the sample kernel and calculate occlusion factor
  float occlusion = 0.0;
  float test = 0.0;
  for (int i = 0; i < 64; ++i) {
    // get sample position
    vec3 samplePos = TBN * samples[i].xyz; // from tangent to view-space
    test = samplePos.z;
    samplePos = fragPos + samplePos * radius;

    // project sample position (to sample texture) (to get position on
    // screen/texture)
    vec4 offset = vec4(samplePos, 1.0);
    offset = vertex_view_to_projection * offset; // from view to clip-space
    offset.xyz /= offset.w;                      // perspective divide
    offset.xyz = offset.xyz * 0.5 + 0.5;         // transform to range 0.0 - 1.0
    // get sample depth
    float sampleDepth =
        texture(gPosition, offset.xy).z; // get depth value of kernel sample

    // range check & accumulate
    float rangeCheck =
        smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
    occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
  }
  occlusion = 1.0 - (occlusion / kernelSize);

  FragColor = vec4(vec3(occlusion), 1.0);
  //   FragColor = occlusion;
}
