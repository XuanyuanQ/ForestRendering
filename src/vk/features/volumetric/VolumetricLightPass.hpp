#pragma once

#include "vk/renderer/IRenderPass.hpp"

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace vkfw
{
  // Minimal-intrusion volumetric light shafts pass.
  // - Reads: set0=CameraUBO, set1=GBuffer textures (incl depth), set2=shadow map
  // - Writes: swapchain color (additive blend)
  class VolumetricLightPass final : public IRenderPass
  {
  public:
    explicit VolumetricLightPass(RenderType render_type = RenderType::Lighting) : IRenderPass(render_type) {}

    bool Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override;
    void Destroy(VkContext &ctx) override;
    void OnSwapchainRecreated(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override;
    void SetupPassLayout(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets, FrameResource &frame_resources) override;
    void Record(FrameContext &frame, RenderTargets &targets) override;
    void setDebugParameter(DebugParam &param) override { debugParameter_ = &param; }

  private:
    vk::raii::PipelineLayout pipeline_layout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};
    DebugParam *debugParameter_ = nullptr;
  };
} // namespace vkfw

