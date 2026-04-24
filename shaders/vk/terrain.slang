// 1. 定义数据结构
struct CameraData {
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 unused_model;
    column_major float4x4 light;
    float4 cameraPos;
    float4 shadowParams; // x: apply shadow (0/1)
};

// 2. 绑定资源 (Binding 0, Set 0)
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
    [[vk::location(2)]] float4 light_clip : TEXCOORD1;
};

// 修复点：HLSL 使用 float2/float3，而不是 vec2/vec3
float waveFun(float time, float A, float f, float p, float k, float2 D, float3 pointPos) {
    // 逻辑修复：D.y 乘的是 pointPos.z (因为地形通常在 XZ 平面)
    float a = sin((D.x * pointPos.x + D.y * pointPos.z) * f + time * p) * 0.5 + 0.5;
    return A * pow(a, k);
}

[shader("vertex")]
VertexOutput vertMain(VertexInput input) {
    VertexOutput output;
    
    // 修复点：变量名统一为 worldPos
    float4 worldPos = float4(input.pos, 1.0);
    float time = 1.0; 
    
    // 计算波浪叠加
    float wave1 = waveFun(time, 4.0, 0.2, 0.5, 2.0, float2(-1.0, 0.0), input.pos);
    float wave2 = waveFun(time, 0.5, 0.4, 1.3, 2.0, float2(-0.7, 0.7), input.pos);

    // 应用高度偏移
    float heightOffset = 1.0 * (wave1 + wave2);
    worldPos.y += heightOffset;
    worldPos.y -=2.50;

    // 变换到裁剪空间 (mul 顺序配合 column_major)
    output.sv_position = mul(ubo.proj, mul(ubo.view, worldPos));
    output.light_clip = mul(ubo.light, worldPos);
    //output.sv_position = float4(input.pos, 1.0);
    
    output.normal = input.normal;
    output.uv = input.uv;
    
    return output;
}

[[vk::binding(1, 0)]] 
Sampler2D texSampler; 

[[vk::binding(2, 0)]]
Sampler2D shadowMap;

float ShadowFactor(float4 lightClip)
{
    float3 proj = lightClip.xyz / max(lightClip.w, 1e-6);
    float2 uv = proj.xy * 0.5 + 0.5;
    float depth = proj.z;

    // outside shadow map -> lit
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || depth < 0.0 || depth > 1.0)
        return 1.0;

    float closest = shadowMap.Sample(uv).r;
    float bias = 0.0015;
    return (depth - bias) > closest ? 0.35 : 1.0;
}

[shader("fragment")]
float4 fragMain(VertexOutput vertIn) : SV_TARGET {
    // 输出红色用于测试
    float4 texColor = texSampler.Sample(vertIn.uv* 50.0);

    //return float4(0.0, vertIn.uv.y, 0.0, 1.0);
    if (ubo.shadowParams.x > 0.5)
    {
        float shadow = ShadowFactor(vertIn.light_clip);
        texColor.rgb *= shadow;
    }
    return texColor;
}
