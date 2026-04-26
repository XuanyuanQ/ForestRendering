#pragma once

#include "vk/renderer/IRenderPass.hpp"

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace vkfw
{
  class SkyboxPass final : public IRenderPass
  {
  public:
    explicit SkyboxPass(RenderType render_type = RenderType::Skybox) : IRenderPass(render_type) {};
    bool Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override;
    void Destroy(VkContext &ctx) override;
    void OnSwapchainRecreated(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override;
    void Record(FrameContext &frame, RenderTargets &targets) override;

  private:
    struct alignas(16) SkyboxUBO
    {
      glm::mat4 world_to_clip{1.0f};
      glm::vec4 light_position{0.0f, 1.0f, 0.0f, 0.0f};
      float time_seconds = 0.0f;
      glm::vec3 _pad{0.0f};
    };

    void CreatePipeline(const vk::raii::Device &device, vk::Format color_format, vk::Format depth_format);
    void CreateDescriptors(VkContext &ctx, uint32_t image_count);

    DescriptorSetInfos ubo_ds_info_{};
    std::vector<vk::raii::Buffer> ubo_buf_{};
    std::vector<vk::raii::DeviceMemory> ubo_mem_{};
    std::vector<void *> ubo_map_{};
    vk::raii::Buffer vertex_buffer_{nullptr};
    vk::raii::DeviceMemory vertex_memory_{nullptr};
  };
} // namespace vkfw

