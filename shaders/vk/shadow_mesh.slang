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

struct VertexInput {
    [[vk::location(0)]] float3 pos : POSITION;
    // 实例属性默认按列对齐插槽
    [[vk::location(3)]] float4x4 instanceModel;
};

[shader("vertex")]
float4 main(VertexInput input) : SV_Position {
    // 计算世界坐标
    float4 worldPos = mul(input.instanceModel, float4(input.pos, 1.0));
    // 变换到裁剪空间
    return mul(ubo.light, worldPos);
}
