// 1. 定义数据结构
struct CameraData {
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 unused_model;
    column_major float4x4 light_world_to_clip_matrix;
    float4 cameraPos;
};

// 2. 绑定资源 (Binding 0, Set 0)
[[vk::binding(0, 0)]]
ConstantBuffer<CameraData> ubo;

// Set=1 binding=0: terrain texture (optional alpha discard)
[[vk::binding(0, 1)]]
Sampler2D alphaTex;

struct VertexInput {
    [[vk::location(0)]] float3 pos : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
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

    float4 worldPos = float4(input.pos, 1.0);
    float time = 1.0; 
    
    // 计算波浪叠加
    float wave1 = waveFun(time, 4.0, 0.2, 0.5, 2.0, float2(-1.0, 0.0), input.pos);
    float wave2 = waveFun(time, 0.5, 0.4, 1.3, 2.0, float2(-0.7, 0.7), input.pos);

    // 应用高度偏移
    float heightOffset = 1.0 * (wave1 + wave2);
    worldPos.y += heightOffset;
    worldPos.y -=2.50;

    output.sv_position = mul(ubo.light_world_to_clip_matrix, worldPos);
    output.uv = input.uv;
    
    return output;
}

[shader("fragment")]
void fragMain(VertexOutput input)
{
    // float a = alphaTex.Sample(input.uv).a;
    // if (a < 0.1)
    //     discard;
}
