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
    explicit ShadowPass(uint32_t shadow_map_res = 2048) : resolution_(shadow_map_res) {};
    // 阴影通道不需要知道 Swapchain，但需要知道阴影贴图的分辨率
    bool Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override;
    void Destroy(VkContext &ctx) override
    {
      ctx.Device().waitIdle();
      for (size_t i = 0; i < ubo_map_.size(); ++i)
      {
        if (i < ubo_mem_.size() && ubo_map_[i])
        {
          ubo_mem_[i].unmapMemory();
          ubo_map_[i] = nullptr;
        }
      }
      ubo_map_.clear();
      ubo_mem_.clear();
      ubo_buf_.clear();
      ds_.clear();
      dp_ = nullptr;
      shadow_sampler_ = nullptr;
      shadow_view_ = nullptr;
      shadow_image_ = nullptr;
      shadow_mem_ = nullptr;
      pipeline_layout_ = nullptr;
      terrain_shadow_pipeline_ = nullptr;
      mesh_shadow_pipeline_ = nullptr;
      pipeline_ = nullptr;
      tex_dsl_ = nullptr;
      dsl_ = nullptr;
    };
    void Record(FrameContext &frame, RenderTargets &targets) override {
      // 阴影通道的 Record 由 Execute 代替，外部（如 TerrainPass）会直接调用 Execute 来传入 Draw 指令
    };
    void OnSwapchainRecreated(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override
    {
      // 阴影通道不依赖交换链资源，所以不需要响应交换链重建
    }

    // 关键：暴露这个函数，供调度者（如 App/Renderer）在 Record 时调用
    // 这样 TerrainPass 就可以把它的 Draw 指令传进来
    void Execute(FrameContext &frame,
                 RenderTargets &targets,
                 const std::function<void(vk::raii::CommandBuffer &cmd, const vk::PipelineLayout &layout)> &draw_callback);

    vk::raii::Pipeline const &GetTerrainShadowPipeline() const { return terrain_shadow_pipeline_; }
    vk::raii::Pipeline const &GetMeshShadowPipeline() const { return mesh_shadow_pipeline_; }
    // 供主渲染通道（Terrain）获取阴影贴图的 View 和 Sampler
    vk::raii::Image const &GetShadowImage() const { return shadow_image_; }
    vk::raii::ImageView const &GetShadowView() const { return shadow_view_; }
    vk::raii::Sampler const &GetShadowSampler() const { return shadow_sampler_; }

  private:
    vk::raii::DescriptorSetLayout CreateDescriptorSetLayout(const vk::raii::Device &device);
    vk::raii::DescriptorSetLayout CreateTextureSetLayout(const vk::raii::Device &device);
    vk::raii::PipelineLayout CreatePipelineLayout(const vk::raii::Device &device, const vk::DescriptorSetLayout &raw_dsl);

    vk::raii::Pipeline CreatePipeline(const vk::raii::Device &device,
                                      vk::Format depth_format,
                                      const std::string &path,
                                      const char *vert_entry,
                                      const char *frag_entry,
                                      bool instanced);
    void CreateShadowResources(VkContext &ctx, uint32_t res);

    vk::raii::DescriptorSetLayout dsl_{nullptr};
    vk::raii::DescriptorSetLayout tex_dsl_{nullptr};
    vk::raii::DescriptorPool dp_{nullptr};
    std::vector<vk::raii::DescriptorSet> ds_{};

    std::vector<vk::raii::Buffer> ubo_buf_{};
    std::vector<vk::raii::DeviceMemory> ubo_mem_{};
    std::vector<void *> ubo_map_{};

    // 阴影资源
    vk::raii::Image shadow_image_{nullptr};
    vk::raii::DeviceMemory shadow_mem_{nullptr};
    vk::raii::ImageView shadow_view_{nullptr};
    vk::raii::Sampler shadow_sampler_{nullptr};
    vk::ImageLayout shadow_layout_{vk::ImageLayout::eUndefined};

    vk::raii::PipelineLayout pipeline_layout_{nullptr};
    vk::raii::Pipeline terrain_shadow_pipeline_ = nullptr; // 专门画地形影子
    vk::raii::Pipeline mesh_shadow_pipeline_ = nullptr;    // 专门画树木影子
    vk::raii::Pipeline pipeline_{nullptr};
    uint32_t resolution_;
  };
}
