#pragma once

#include <cstddef> // offsetof
#include <array>
#include <vector>

#include <glm/glm.hpp>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan.hpp> // 只用到 vk::Format/描述符（非 raii 也行）
#else
import vulkan_hpp;
#endif

namespace vkfw {

struct Vertex {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec2 uv;
};
struct InstanceData {
  glm::mat4 model; // 包含位置、旋转、缩放
};
// Vulkan 顶点输入描述：给 pipeline 用
inline std::vector<vk::VertexInputBindingDescription> VertexBindingDescriptions()
{
  return {
      // Binding 0: 逐顶点数据 (树的几何形状)
      vk::VertexInputBindingDescription{0u, sizeof(Vertex), vk::VertexInputRate::eVertex},
      // Binding 1: 逐实例数据 (森林中每棵树的位置)
      vk::VertexInputBindingDescription{1u, sizeof(InstanceData), vk::VertexInputRate::eInstance}
  };
}

// 修改：增加对 mat4 的属性描述
inline std::vector<vk::VertexInputAttributeDescription> VertexAttributeDescriptions()
{
  std::vector<vk::VertexInputAttributeDescription> attrs;

  // Binding 0: 顶点属性 (Location 0, 1, 2)
  attrs.push_back({0u, 0u, vk::Format::eR32G32B32Sfloat, static_cast<uint32_t>(offsetof(Vertex, pos))});
  attrs.push_back({1u, 0u, vk::Format::eR32G32B32Sfloat, static_cast<uint32_t>(offsetof(Vertex, normal))});
  attrs.push_back({2u, 0u, vk::Format::eR32G32Sfloat, static_cast<uint32_t>(offsetof(Vertex, uv))});

  // Binding 1: Instance 属性 (Location 3, 4, 5, 6)
  // 因为 Vulkan 中一个 location 最多传 vec4，所以 mat4 必须拆成 4 个 location
  for (uint32_t i = 0; i < 4; ++i) {
    attrs.push_back({
        3u + i,                                          // Location: 3, 4, 5, 6
        1u,                                              // Binding: 1
        vk::Format::eR32G32B32A32Sfloat,                 // 每一列都是 vec4
        static_cast<uint32_t>(offsetof(InstanceData, model) + sizeof(glm::vec4) * i)
    });
  }

  return attrs;
}

} // namespace vkfw
