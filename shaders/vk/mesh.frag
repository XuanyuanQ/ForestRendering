#version 450

layout(location=0) in vec3 vN;
layout(location=1) in vec2 vUV; // 新增：接收从 Vertex Shader 传来的 UV 坐标

// 新增：接收贴图采样器 (binding=1)
layout(set=0, binding=1) uniform sampler2D texSampler;

layout(location=0) out vec4 outColor;

void main() {
    // 1. 采样贴图颜色
    vec4 texColor = texture(texSampler, vUV);

    // 2. Alpha 测试（透明剔除）：这是树叶变成树叶而不是方块的关键！
    if (texColor.a < 0.1) {
        discard; 
    }

    // 3. 简单的漫反射光照（让树有立体感，暂时假设光从正上方偏一点打过来）
    vec3 n = normalize(vN);
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3)); 
    float diff = max(dot(n, lightDir), 0.0);
    
    // 基础环境光 + 漫反射
    vec3 ambient = 0.3 * texColor.rgb;
    vec3 diffuse = diff * texColor.rgb;

    outColor = vec4(ambient + diffuse, 1.0);
}