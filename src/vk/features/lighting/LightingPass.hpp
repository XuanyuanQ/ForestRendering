#pragma once

#include "vk/renderer/IRenderPass.hpp"

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace vkfw {
struct Vertex {
  float pos[2];
  float color[3];
};
class LightingPass final : public IRenderPass {
public:
  bool Create(VkContext& ctx, VkSwapchain const& swapchain, RenderTargets& targets) override;
  void Destroy(VkContext& ctx) override;
  void OnSwapchainRecreated(VkContext& ctx, VkSwapchain const& swapchain, RenderTargets& targets) override;
  void Record(FrameContext& frame, RenderTargets& targets) override;
  void setDebugParameter(DebugParam& param) override;
private:
	void updateVertexBuffer();
private:
  vk::raii::PipelineLayout pipeline_layout_{nullptr};
  vk::raii::Pipeline pipeline_{nullptr};
  vk::raii::Buffer vertex_buffer_{nullptr};
  vk::raii::DeviceMemory vertex_memory_{nullptr};
  vk::raii::Buffer index_buffer_{nullptr};
  vk::raii::DeviceMemory index_memory_{nullptr};
  std::array<Vertex, 3>  vertices_;
  DebugParam* debugParameter_ = nullptr;
};

} // namespace vkfw
