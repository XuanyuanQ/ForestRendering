#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <array>
#include <vulkan/vulkan.hpp>
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace vkfw
{

    struct ImGuiResources
    {
        vk::raii::DescriptorPool descriptorPool = nullptr;
    };

    struct alignas(16) CameraUBO
    {
        glm::mat4 view{1.0f};
        glm::mat4 proj{1.0f};
        glm::mat4 model{1.0f};
        glm::mat4 light{1.0f};
        glm::vec4 camera_pos{0.0f};
        glm::vec4 shadow_params{1.0f, 0.0f, 0.0f, 0.0f}; // x: apply shadow (0/1)
        glm::vec4 light_dir{0.0f, 1.0f, 0.0f, 0.0f};     // xyz: world-space direction to light (directional)
    };

    enum class RenderType {
        UI,         // UI 元素（通常最后画，开启混合）
        Skybox,      // 天空盒（通常最后画或最先画，关闭深度写入）
        Opaque,      // 普通不透明物体（Terrain, Mesh）
        Shadow,      // 阴影贴图生成（只写深度，不写颜色）
        Lighting,    // 灯光处理/后处理
        Transparent  // 透明物体（需要排序）
        };

    struct DebugParam
    {
        bool animation{false};
        bool volumtricl{false};
        bool shadowmap{false};
        float daySpeed = 0.5f;
        float lightX{-52.8f};
        float lightY{50.4f};
        float lightZ{100.0f};
    };

} // namespace Forest
