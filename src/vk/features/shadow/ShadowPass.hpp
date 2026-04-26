#pragma once
#include "vk/renderer/IRenderPass.hpp"
#include <glm/glm.hpp>

#include <functional>
#include <vector>

namespace vkfw
{
  class ShadowPass final : public IRenderPass
  {
  public:
    explicit ShadowPass(RenderType render_type = RenderType::Shadow, uint32_t shadow_map_res = 2048) : IRenderPass(render_type), resolution_(shadow_map_res) {};
    bool Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override;
    void Destroy(VkContext &ctx) override
    {
      ctx.Device().waitIdle();
      // Shadow 贴图资源
      shadow_sampler_ = nullptr;
      shadow_view_ = nullptr;
      shadow_image_ = nullptr;
      shadow_mem_ = nullptr;
      // 重构后 pass_resources_ 清理（布局/材质描述符）
      ClearPassResources();
    };
    void Record(FrameContext &frame, RenderTargets &targets) override {
      (void)frame;
      (void)targets;
    };
    bool CastsShadow() const override { return false; }
    void OnSwapchainRecreated(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override
    {
      (void)ctx;
      (void)swapchain;
      (void)targets;
    }

    void Execute(FrameContext &frame,
                 RenderTargets &targets,
                 const std::function<void(vk::raii::CommandBuffer &cmd, const vk::PipelineLayout &layout)> &draw_callback);

  private:
    void CreateShadowResources(VkContext &ctx, uint32_t res);

    vk::raii::Image shadow_image_{nullptr};
    vk::raii::DeviceMemory shadow_mem_{nullptr};
    vk::raii::ImageView shadow_view_{nullptr};
    vk::raii::Sampler shadow_sampler_{nullptr};
    vk::ImageLayout shadow_layout_{vk::ImageLayout::eUndefined};

    uint32_t resolution_;
  };
}
