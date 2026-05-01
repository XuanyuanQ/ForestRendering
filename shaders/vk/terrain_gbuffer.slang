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

float waveFun(float time, float A, float f, float p, float k, float2 D, float3 pointPos) {
    float a = sin((D.x * pointPos.x + D.y * pointPos.z) * f + time * p) * 0.5 + 0.5;
    return A * pow(a, k);
}

[shader("vertex")]
VertexOutput vertMain(VertexInput input) {
    VertexOutput output;

    float4 worldPos = float4(input.pos, 1.0);
    float time = 1.0;
    float wave1 = waveFun(time, 4.0, 0.2, 0.5, 2.0, float2(-1.0, 0.0), input.pos);
    float wave2 = waveFun(time, 0.5, 0.4, 1.3, 2.0, float2(-0.7, 0.7), input.pos);
    worldPos.y += (wave1 + wave2);
    worldPos.y -= 2.50;

    output.sv_position = mul(ubo.proj, mul(ubo.view, worldPos));
    output.normal = normalize(input.normal);
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
GBufferOutput fragMain(VertexOutput input) {
    float4 texColor = texSampler.Sample(input.uv * 50.0);

    GBufferOutput output;
    output.diffuse = float4(texColor.rgb, 1.0);
    output.specular = float4(0.03, 0.03, 0.03, 1.0);
    output.normal = float4(normalize(input.normal) * 0.5 + 0.5, 1.0);
    return output;
}
