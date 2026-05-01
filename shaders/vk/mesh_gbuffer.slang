struct CameraData {
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 unused_model;
    column_major float4x4 light;
    float4 cameraPos;
    float4 shadowParams;
    float4 lightDir;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraData> ubo;

struct VertexInput {
    [[vk::location(0)]] float3 pos : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
    [[vk::location(3)]] float4x4 instanceModel;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    [[vk::location(0)]] float3 normal : NORMAL;
    [[vk::location(1)]] float2 uv : TEXCOORD0;
};

struct GBufferOutput {
    [[vk::location(0)]] float4 diffuse : SV_Target0;
    [[vk::location(1)]] float4 specular : SV_Target1;
    [[vk::location(2)]] float4 normal : SV_Target2;
};

[[vk::binding(0, 1)]]
Sampler2D texSampler;

[shader("vertex")]
VertexOutput vertMain(VertexInput input) {
    VertexOutput output;
    float4 worldPos = mul(input.instanceModel, float4(input.pos, 1.0));
    output.sv_position = mul(ubo.proj, mul(ubo.view, worldPos));
    output.normal = normalize(mul((float3x3)input.instanceModel, input.normal));
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
GBufferOutput fragMain(VertexOutput input) {
    float4 texColor = texSampler.Sample(input.uv);
    if (texColor.a < 0.1) {
        discard;
    }

    GBufferOutput output;
    output.diffuse = float4(texColor.rgb, 1.0);
    output.specular = float4(0.08, 0.08, 0.08, 1.0);
    output.normal = float4(normalize(input.normal) * 0.5 + 0.5, 1.0);
    return output;
}
