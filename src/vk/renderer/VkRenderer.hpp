#pragma once

#include <memory>
#include <vector>
#include "vk/scene/common.hpp"
#include "vk/renderer/FrameContext.hpp"
#include "vk/renderer/RenderTargets.hpp"
#include "vk/renderer/FrameGlobals.hpp"

namespace vkfw
{

  class VkContext;
  class VkSwapchain;
  class VkFrameSync;
  class IRenderPass;
  class ShadowPass;

  class VkRenderer
  {
  public:
    VkRenderer() = default;
    ~VkRenderer() = default;

    VkRenderer(VkRenderer const &) = delete;
    VkRenderer &operator=(VkRenderer const &) = delete;
    void AddObjectPass(std::unique_ptr<IRenderPass> pass);
    void setShowDepthPass(std::unique_ptr<ShadowPass> pass);
    void AddPass(std::unique_ptr<IRenderPass> pass);

    bool Create(VkContext &ctx, VkSwapchain &swapchain, VkFrameSync &sync, DebugParam &param);
    void Destroy(VkContext &ctx);
    void OnSwapchainRecreated(VkContext &ctx, VkSwapchain &swapchain, VkFrameSync &sync);

    // Returns false when swapchain needs recreation.
    bool DrawFrame(VkContext &ctx, VkSwapchain &swapchain, VkFrameSync &sync, FrameGlobals const &globals);

    RenderTargets &Targets() noexcept { return targets_; }
    RenderTargets const &Targets() const noexcept { return targets_; }

  private:
    void CreateCommandResources(VkContext &ctx, VkFrameSync &sync);
    void DestroyCommandResources(VkContext &ctx);
    void CreateSharedDepth(VkContext &ctx, vk::Extent2D extent);
    void DestroySharedDepth(VkContext &ctx);
    void SyncSharedDepthTargets() noexcept;
    void CreateFrameResources(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets);
    void RefreshFrameShadowDescriptors(VkContext &ctx, VkSwapchain const &swapchain);
    template <typename Fn>
    void ForEachPassByType(RenderType type, Fn &&fn)
    {
      for (auto &pass : pass_nodes_)
      {
        if (pass->GetRenderType() != type)
          continue;
        fn(*pass);
      }
    }

    std::vector<std::unique_ptr<IRenderPass>> pass_nodes_;
    RenderTargets targets_{};

    vk::raii::CommandPool command_pool_{nullptr};
    std::vector<vk::raii::CommandBuffer> command_buffers_{};
    uint32_t frame_index_ = 0;

    vk::Format shared_depth_format_{vk::Format::eUndefined};
    vk::ImageLayout shared_depth_layout_{vk::ImageLayout::eUndefined};
    vk::Extent2D shared_depth_extent_{};
    vk::raii::Image shared_depth_img_{nullptr};
    vk::raii::DeviceMemory shared_depth_mem_{nullptr};
    vk::raii::ImageView shared_depth_view_{nullptr};

    FrameResource frame_resources_;
    vk::raii::Sampler common_sampler_{nullptr};
  };

} // namespace vkfw
