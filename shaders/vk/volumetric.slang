struct CameraData {
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 model;
    column_major float4x4 light;
    float4 cameraPos;
    float4 shadowParams; // x: apply shadow map
    float4 lightDir;     // xyz: direction to light
    column_major float4x4 viewInv;
    column_major float4x4 projInv;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraData> ubo;

// GBuffer set=1: same bindings as lighting.slang
[[vk::binding(0, 1)]] Sampler2D gbufferDiffuse;
[[vk::binding(1, 1)]] Sampler2D gbufferSpecular;
[[vk::binding(2, 1)]] Sampler2D gbufferNormal;
[[vk::binding(3, 1)]] Sampler2D gbufferDepth;

[[vk::binding(0, 2)]]
Sampler2D shadowMap;

struct VertexOutput {
    float4 sv_position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertMain(uint vertexID : SV_VertexID) {
    static const float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };

    static const float2 uvs[3] = {
        float2(0.0, 0.0),
        float2(0.0, 2.0),
        float2(2.0, 0.0)
    };

    VertexOutput output;
    output.sv_position = float4(positions[vertexID], 0.0, 1.0);
    output.uv = uvs[vertexID];
    return output;
}

float Random(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

float GetMiePhase(float g, float cosTheta)
{
    float g2 = g * g;
    float denom = max(1.0 + g2 - 2.0 * g * cosTheta, 0.0001);
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(denom, 1.5));
}

float GetAdaptiveIntensity(float3 currentPos, float3 sunDir, float3 rayDir)
{
    float height = max(0.0, currentPos.y);
    float heightAtten = 1.0 - smoothstep(0.0, 25.0, height) * 0.8;

    float lookAtSun = dot(rayDir, sunDir);
    float angleAtten = 1.0 - smoothstep(0.8, 1.0, lookAtSun) * 0.8;
    return heightAtten * angleAtten;
}

float ShadowFactor(float3 worldPos)
{
    float4 lightClip = mul(ubo.light, float4(worldPos, 1.0));
    float3 proj = lightClip.xyz / max(lightClip.w, 1e-6);
    float2 uv = proj.xy * 0.5 + 0.5;
    float currentDepth = proj.z;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || currentDepth < 0.0 || currentDepth > 1.0)
        return 1.0;

    float bias = 0.003 / max(lightClip.w, 1.0);

    uint width, height;
    shadowMap.GetDimensions(width, height);
    float2 texel = 1.0 / float2((float)width, (float)height);

    float shadowSum = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closestDepth = shadowMap.Sample(uv + float2(x, y) * texel).r;
            if (currentDepth - bias > closestDepth)
                shadowSum += 1.0;
        }
    }
    return 1.0 - shadowSum / 9.0;
}

float3 GetSunColor(float sunY)
{
    float t = saturate((sunY + 0.1) / 0.9);
    float3 warm = float3(1.0, 0.85, 0.65);
    float3 white = float3(1.0, 0.97, 0.90);
    return lerp(warm, white, t);
}

bool ComputeSunUV(out float2 sunUV)
{
    // Project a far point in the sun direction from the camera.
    float3 sunDir = normalize(ubo.lightDir.xyz);
    float3 p = ubo.cameraPos.xyz + sunDir * 1000.0;
    float4 clip = mul(ubo.proj, mul(ubo.view, float4(p, 1.0)));
    if (clip.w <= 1e-6)
    {
        sunUV = float2(-10.0, -10.0);
        return false;
    }
    float2 ndc = clip.xy / clip.w; // [-1,1]
    sunUV = ndc * 0.5 + 0.5;       // [0,1]
    return (sunUV.x >= 0.0 && sunUV.x <= 1.0 && sunUV.y >= 0.0 && sunUV.y <= 1.0);
}

[shader("fragment")]
float4 fragMain(VertexOutput input) : SV_Target
{
    float3 sunDir = normalize(ubo.lightDir.xyz);

    // Day/night gating.
    float sunUp = saturate((sunDir.y + 0.05) / 0.25);
    if (sunUp <= 0.001)
        return float4(0.0, 0.0, 0.0, 0.0);

    float2 sunUV;
    bool sunOnScreen = ComputeSunUV(sunUV);
    if (!sunOnScreen)
        return float4(0.0, 0.0, 0.0, 0.0);

    // Screen-space crepuscular rays (radial sampling towards the sun).
    // This produces "shafts" shaped by occluders instead of a circular fog patch.
    const int SAMPLES = 64;
    const float DENSITY = 0.95;
    const float DECAY = 0.97;
    const float WEIGHT = 0.55;
    const float EXPOSURE = 1.35;

    float2 delta = (sunUV - input.uv) * (DENSITY / float(SAMPLES));
    float jitter = Random(input.uv * 2048.0 + float2(sin(sunDir.x), cos(sunDir.z)));
    float2 sampleUV = input.uv + delta * jitter;

    float illum = 0.0;
    float w = 1.0;
    for (int i = 0; i < SAMPLES; ++i)
    {
        sampleUV += delta;

        float depth = gbufferDepth.Sample(sampleUV).r;
        // Visibility along the ray to the sun. 1 = lit, 0 = blocked.
        float vis = 1.0;
        if (depth < 0.99999)
        {
            float4 clipPos = float4(sampleUV * 2.0 - 1.0, depth, 1.0);
            float4 viewPos = mul(ubo.projInv, clipPos);
            viewPos /= max(viewPos.w, 1e-6);
            float4 worldPos4 = mul(ubo.viewInv, viewPos);
            float3 worldPos = worldPos4.xyz;

            if (ubo.shadowParams.x > 0.5)
                vis = ShadowFactor(worldPos);
        }

        illum += vis * w * WEIGHT;
        w *= DECAY;
    }

    // Concentrate near the sun to avoid a uniform screen haze.
    float distToSun = length(input.uv - sunUV);
    float falloff = smoothstep(1.0, 0.0, distToSun * 1.6);

    // Normalize by sample count so exposure behaves predictably.
    float intensity = (illum / float(SAMPLES)) * EXPOSURE * sunUp * falloff;
    float3 sunColor = GetSunColor(sunDir.y);
    float3 color = sunColor * intensity;
    color = color / (1.0 + color); // simple tonemap

    // Alpha controls additive strength (pipeline uses SrcAlpha + One).
    float alpha = saturate(intensity * 0.65);
    return float4(color, alpha);
}
