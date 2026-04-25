// 0. UBO (must match C++ `vkfw::CameraUBO` / vertex shader CameraData)
struct CameraData {
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 unused_model;
    column_major float4x4 light;
    float4 cameraPos;
    float4 shadowParams; // x: apply shadow (0/1)
    float4 lightDir;     // xyz: world-space direction to light
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
[[vk::binding(0, 1)]] 
Sampler2D texSampler; 

[[vk::binding(0, 2)]]
Sampler2D shadowMap;

// float ShadowFactor(float4 lightClip)
// {
//     float3 proj = lightClip.xyz / max(lightClip.w, 1e-6);
//     float2 uv = proj.xy * 0.5 + 0.5;
//     float depth = proj.z;

//     if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || depth < 0.0 || depth > 1.0)
//         return 1.0;

//     float closest = shadowMap.Sample(uv).r;
//     float bias = 0.002;
//     return (depth - bias) > closest ? 0.35 : 1.0;
// }

float ShadowFactor(float4 lightClip)
{
    float3 proj = lightClip.xyz / max(lightClip.w, 1e-6);
    float2 uv = proj.xy * 0.5 + 0.5;
    // float2 uv = proj.xy * float2(0.5, -0.5) + 0.5;
    float current_depth = proj.z;

    // outside shadow map -> lit
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || current_depth < 0.0 || current_depth > 1.0)
        return 1.0;
    // float bias = 0.002;
    float bias = 0.005 / lightClip.w;
    uint width, height;
    shadowMap.GetDimensions(width, height); // 获取 0 级 mip 的宽高
    float2 shadowmap_texel_size = 1.0 / float2((float)width, (float)height);
    float shadow_sum = 0.0;
    for (int i = -2; i <= 2; ++i) {
        for (int j = -2; j <= 2; ++j) {
            // Sample the ShadowMap (values ​​between 0.0 and 1.0).
            float closest_depth = shadowMap.Sample(uv+  float2(i, j) * shadowmap_texel_size).r;
            if (current_depth - bias > closest_depth)
                shadow_sum += 1.0;
        }
    }
    float shadow_factor = shadow_sum / 25.0;
    // return shadowMap.Sample(uv).r;
    return 1.0 - shadow_factor;
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
    float3 lightDir = normalize(ubo.lightDir.xyz);
    
    // dot() 函数在 HLSL 中返回标量
    float diff = max(dot(n, lightDir), 0.0);
    
    // 基础环境光 + 漫反射
    float3 ambient = 0.3 * texColor.rgb;
    float3 diffuse = diff * texColor.rgb;

    float shadow = 1.0;
    if (ubo.shadowParams.x > 0.5)
        shadow = ShadowFactor(input.light_clip);

    // Keep ambient in shadow; only attenuate direct lighting.
    float3 lit = ambient + diffuse * shadow;
    return float4(lit, 1.0);
}
