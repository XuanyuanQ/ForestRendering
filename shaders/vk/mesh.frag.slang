// 0. UBO (must match C++ `vkfw::CameraUBO` / vertex shader CameraData)
struct CameraData {
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 unused_model;
    column_major float4x4 light;
    float4 cameraPos;
    float4 shadowParams; // x: apply shadow (0/1)
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraData> ubo;

// 1. 定义输入结构体 (必须与 Vertex Shader 的输出结构体 location 对应)
struct FragmentInput
{
    [[vk::location(0)]] float3 normal : NORMAL;
    [[vk::location(1)]] float2 uv     : TEXCOORD0;
    [[vk::location(2)]] float4 light_clip : TEXCOORD1;
};

// 2. 定义资源绑定
// 在 Slang 中，Texture 和 Sampler 通常成对使用
// 如果你的 Vulkan 代码使用的是 Combined Image Sampler，就这样写：
[[vk::binding(1, 0)]] 
Sampler2D texSampler; 

[[vk::binding(2, 0)]]
Sampler2D shadowMap;

float ShadowFactor(float4 lightClip)
{
    float3 proj = lightClip.xyz / max(lightClip.w, 1e-6);
    float2 uv = proj.xy * 0.5 + 0.5;
    float depth = proj.z;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || depth < 0.0 || depth > 1.0)
        return 1.0;

    float closest = shadowMap.Sample(uv).r;
    float bias = 0.002;
    return (depth - bias) > closest ? 0.35 : 1.0;
}

// 3. 片段着色器入口
[shader("fragment")]
float4 main(FragmentInput input) : SV_Target
{
    // 1. 采样贴图颜色
    // Slang/HLSL 使用 .Sample(sampler, uv) 语法
    float4 texColor = texSampler.Sample(input.uv);

    // 2. Alpha 测试（透明剔除）
    // 这是处理树叶、草丛等植被的核心逻辑
    if (texColor.a < 0.1)
    {
        discard;
    }

    // 3. 简单的漫反射光照计算
    float3 n = normalize(input.normal);
    float3 lightDir = normalize(float3(0.5, 1.0, 0.3)); 
    
    // dot() 函数在 HLSL 中返回标量
    float diff = max(dot(n, lightDir), 0.0);
    
    // 基础环境光 + 漫反射
    float3 ambient = 0.3 * texColor.rgb;
    float3 diffuse = diff * texColor.rgb;

    float3 lit = ambient + diffuse;
    if (ubo.shadowParams.x > 0.5)
    {
        float shadow = ShadowFactor(input.light_clip);
        lit *= shadow;
    }
    return float4(lit, 1.0);
}
