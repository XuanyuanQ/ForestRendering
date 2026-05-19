#pragma once
#include "vk/scene/Model.hpp"
#include "vk/renderer/IRenderPass.hpp"
#include "vk/renderer/FrameContext.hpp"

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace vkfw
{
  // 假设我们定义一个简单的结构体来传每个实例的偏移量
  struct TerrainInstanceData {
      glm::mat4 model; // 🟢 变成 64 字节的完整矩阵，跟 Shader 的 float4x4 严格对应
  };
  class TerrainPass final : public IRenderPass
  {
  public:
    explicit TerrainPass(RenderType render_type = RenderType::Opaque) : IRenderPass(render_type) {};    
    bool Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override;
    void Destroy(VkContext &ctx) override;
    void OnSwapchainRecreated(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override;
    void Record(FrameContext &frame, RenderTargets &targets) override;
    void JustDraw(FrameContext &frame, vk::raii::CommandBuffer &cmd, vk::PipelineLayout layout, uint32_t image_index) override;
    void setDebugParameter(DebugParam &param) override { debugParameter_ = &param; }

  private:
  void CreateInstanceBuffer(
    const VkContext &ctx,
    const std::vector<TerrainInstanceData>& instance_data);
    void CreateVertexBuffer(
        const VkContext &ctx,
        const vk::PhysicalDeviceMemoryProperties &mem_props,
        const Model &model);
    void CreateIndexBuffer(
        const VkContext &ctx,
        const vk::PhysicalDeviceMemoryProperties &mem_props,
        const Model &model);

  private:
    vk::raii::Buffer vertex_buffer_{nullptr};
    vk::raii::DeviceMemory vertex_memory_{nullptr};
    vk::raii::Buffer index_buffer_{nullptr};
    vk::raii::DeviceMemory index_memory_{nullptr};
    Model terrtain_;
    // 在 TerrainPass 类中增加以下成员变量
    vk::raii::Buffer        instance_buffer_ = nullptr;
    vk::raii::DeviceMemory  instance_memory_ = nullptr;
    uint32_t                instance_count_  = 0;
    uint32_t index_count_ = 0;
     std::vector<TerrainInstanceData> instance_matrices_;


  };

} // namespace vkfw
