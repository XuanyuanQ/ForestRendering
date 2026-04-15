#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

// Instance 数据：一个 mat4 占用 4 个 location (3, 4, 5, 6)
layout(location = 3) in vec4 iCol0;
layout(location = 4) in vec4 iCol1;
layout(location = 5) in vec4 iCol2;
layout(location = 6) in vec4 iCol3;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 unused_model; // 原来的 model 矩阵现在没用了
    vec4 cameraPos;
} ubo;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUV;

void main() {
    // 组装当前实例的模型矩阵
    mat4 instanceModel = mat4(iCol0, iCol1, iCol2, iCol3);
    
    // 计算世界空间坐标
    vec4 worldPos = instanceModel * vec4(inPos, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    vNormal = mat3(instanceModel) * inNormal;
    vUV = inUV;
}