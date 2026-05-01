struct CameraData {
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 unused_model;
    column_major float4x4 light;
    float4 cameraPos;
    float4 shadowParams;
    float4 lightDir;
    column_major float4x4 viewInv;
    column_major float4x4 projInv;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraData> ubo;

[[vk::binding(0, 1)]]
Sampler2D gbufferDiffuse;

[[vk::binding(1, 1)]]
Sampler2D gbufferSpecular;

[[vk::binding(2, 1)]]
Sampler2D gbufferNormal;

[[vk::binding(3, 1)]]
Sampler2D gbufferDepth;

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

float ShadowFactor(float4 worldPos) {
    float4 lightClip = mul(ubo.light, worldPos);
    float3 proj = lightClip.xyz / max(lightClip.w, 1e-6);
    float2 uv = proj.xy * 0.5 + 0.5;
    float currentDepth = proj.z;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || currentDepth < 0.0 || currentDepth > 1.0)
        return 1.0;

    float bias = 0.005 / max(lightClip.w, 1.0);
    uint width, height;
    shadowMap.GetDimensions(width, height);
    float2 texel = 1.0 / float2((float)width, (float)height);

    float shadowSum = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            float closestDepth = shadowMap.Sample(uv + float2(x, y) * texel).r;
            if (currentDepth - bias > closestDepth)
                shadowSum += 1.0;
        }
    }
    return 1.0 - shadowSum / 25.0;
}

[shader("fragment")]
float4 fragMain(VertexOutput input) : SV_Target {
    float depth = gbufferDepth.Sample(input.uv).r;
    if (depth >= 0.99999) {
        discard;
    }

    float3 albedo = gbufferDiffuse.Sample(input.uv).rgb;
    float3 encodedNormal = gbufferNormal.Sample(input.uv).rgb;
    float3 normal = normalize(encodedNormal * 2.0 - 1.0);
    float specularStrength = gbufferSpecular.Sample(input.uv).r;

    float4 clipPos = float4(input.uv * 2.0 - 1.0, depth, 1.0);
    float4 viewPos = mul(ubo.projInv, clipPos);
    viewPos /= max(viewPos.w, 1e-6);
    float4 worldPos = mul(ubo.viewInv, viewPos);

    float3 lightDir = normalize(ubo.lightDir.xyz);
    float3 viewDir = normalize(ubo.cameraPos.xyz - worldPos.xyz);
    float3 halfDir = normalize(lightDir + viewDir);
    float ambient = 0.25;
    float diff = max(dot(normal, lightDir), 0.0);
    float spec = pow(max(dot(normal, halfDir), 0.0), 32.0) * specularStrength;

    float shadow = 1.0;
    if (ubo.shadowParams.x > 0.5) {
        shadow = ShadowFactor(worldPos);
    }

    float3 color = albedo * (ambient + diff * shadow) + spec * shadow;
    return float4(color, 1.0);
}
