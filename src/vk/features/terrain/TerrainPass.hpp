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

  class TerrainPass final : public IRenderPass
  {
  public:
    explicit TerrainPass(RenderType render_type = RenderType::Opaque) : IRenderPass(render_type) {};    
    bool Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override;
    void Destroy(VkContext &ctx) override;
    void OnSwapchainRecreated(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override;
    void Record(FrameContext &frame, RenderTargets &targets) override;
    void JustDraw(FrameContext &frame, vk::raii::CommandBuffer &cmd, vk::PipelineLayout layout, uint32_t image_index) override;
    void RecordShadow(FrameContext &frame, vk::raii::CommandBuffer &cmd, vk::PipelineLayout layout, uint32_t image_index) override;
    void setDebugParameter(DebugParam &param) override { debugParameter_ = &param; }

  private:
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
    DebugParam *debugParameter_ = nullptr;
  };

} // namespace vkfw
