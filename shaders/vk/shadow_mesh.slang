// 1. 定义数据结构，直接使用关键字修饰
struct CameraData {
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 unused_model;
    column_major float4x4 light;
    float4 cameraPos;
};

// 2. 绑定资源
[[vk::binding(0, 0)]]
ConstantBuffer<CameraData> ubo;

// Set=1 binding=0: alpha texture for shadow discard
[[vk::binding(0, 1)]]
Sampler2D alphaTex;

struct VertexInput {
    [[vk::location(0)]] float3 pos : POSITION;
    // 实例属性默认按列对齐插槽
    [[vk::location(3)]] float4x4 instanceModel;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
};

struct VertexOutput {
    float4 sv_position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput input) {
    VertexOutput output;
    // 计算世界坐标
    float4 worldPos = mul(input.instanceModel, float4(input.pos, 1.0));
    // 变换到裁剪空间
    output.sv_position = mul(ubo.light, worldPos);
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
void fragMain(VertexOutput input)
{
    float a = alphaTex.Sample(input.uv).a;
    // if (a < 0.1)
    //     discard;
}
