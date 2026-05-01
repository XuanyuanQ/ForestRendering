#pragma once

#include "vk/renderer/IRenderPass.hpp"

namespace vkfw
{

  struct GBufferAttachmentResource
  {
    vk::Format format{vk::Format::eUndefined};
    vk::ImageLayout layout{vk::ImageLayout::eUndefined};
    vk::raii::Image image{nullptr};
    vk::raii::DeviceMemory memory{nullptr};
    vk::raii::ImageView view{nullptr};
  };

  struct GBufferResources
  {
    vk::Extent2D extent{};
    GBufferAttachmentResource diffuse{};
    GBufferAttachmentResource specular{};
    GBufferAttachmentResource normal{};
    GBufferAttachmentResource depth{};
    vk::raii::Sampler sampler{nullptr};
  };

  class GBufferPass final : public IRenderPass
  {
  public:
    explicit GBufferPass(RenderType render_type = RenderType::GBuffer) : IRenderPass(render_type) {}; 

    bool Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override;
    void Destroy(VkContext &ctx) override;
    void OnSwapchainRecreated(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override;
    void Record(FrameContext &frame, RenderTargets &targets) override;
    void Execute(FrameContext &frame,
                 RenderTargets &targets,
                 const std::function<void(vk::raii::CommandBuffer &cmd, const vk::PipelineLayout &layout)> &draw_callback) override;

  private:
    GBufferResources CreateGBufferResources(VkContext &ctx, vk::Extent2D extent);
    void PublishTargets(RenderTargets &targets) const;
    void ClearTargets(RenderTargets &targets) const;

    GBufferResources gbuffer_{};
  };

} // namespace vkfw
