#pragma once
#include "vk/scene/common.hpp"
#include "vk/renderer/RenderTargets.hpp"
#include "vk/renderer/helper.hpp"
#include "vk/core/VkContext.hpp"
#include "vk/renderer/FrameContext.hpp"
#include <functional>
namespace vkfw
{

  class VkContext;
  class VkSwapchain;
  struct FrameContext;
  struct RenderTargets;
  // struct PassResource;
  // struct FrameResource;
  // struct DebugParam;

  class IRenderPass
  {
  public:
  explicit IRenderPass(RenderType render_type = RenderType::Opaque) 
        : render_type_(render_type) {}

    virtual ~IRenderPass() = default;

    virtual bool Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) = 0;

    virtual void Destroy(VkContext &ctx)
    {
      ctx.Device().waitIdle();
      common_sampler_ = nullptr;
      textures_.clear();
    }

    virtual void OnSwapchainRecreated(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) = 0;
    virtual void Execute(FrameContext &frame,
                 RenderTargets &targets,
                 const std::function<void(vk::raii::CommandBuffer &cmd, const vk::PipelineLayout &layout)> &draw_callback){};
    virtual void Record(FrameContext &frame, RenderTargets &targets) = 0;
    virtual void JustDraw(FrameContext &frame, vk::raii::CommandBuffer &cmd, vk::PipelineLayout layout, uint32_t image_index) {};
    virtual void RecordShadow(FrameContext &frame, vk::raii::CommandBuffer &cmd, vk::PipelineLayout layout, uint32_t image_index)
    {
      JustDraw(frame, cmd, layout, image_index);
    }
    virtual bool CastsShadow() const { return render_type_ == RenderType::Opaque; }
    virtual void setDebugParameter(DebugParam &param) {};
    RenderType GetRenderType() const { return render_type_; }
    PassResource const &GetPassResource() const noexcept { return pass_resources_; }  
    void SetupPassLayout(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets, FrameResource &frame_resources);

  protected:
    vk::raii::Pipeline CreateColorPipeline(
        const vk::raii::Device &device,
        const std::string &shader_path,
        vk::Format color_format,
        vk::Format depth_format);
    vk::raii::Pipeline CreateDepthPipeline(
        const vk::raii::Device &device,
        const std::string &shader_path,
        vk::Format color_format,
        vk::Format depth_format);

    TextureResource LoadTextureResource(
        vk::raii::Device &device,
        vk::raii::PhysicalDevice &physDevice,
        vk::raii::Queue &queue,
        const std::string &path);
    std::vector<char> ReadFile(std::string const &filename);
    void LoadTexture(VkContext &ctx, const std::string &path, uint32_t index);

    void CreateCommonSampler(vk::raii::Device &device)
    {
      if (*common_sampler_ != VK_NULL_HANDLE)
        return;
      vk::SamplerCreateInfo sampler_ci{};
      sampler_ci.magFilter = vk::Filter::eLinear;
      sampler_ci.minFilter = vk::Filter::eLinear;
      sampler_ci.addressModeU = vk::SamplerAddressMode::eRepeat;
      sampler_ci.addressModeV = vk::SamplerAddressMode::eRepeat;
      sampler_ci.addressModeW = vk::SamplerAddressMode::eRepeat;
      common_sampler_ = vk::raii::Sampler{device, sampler_ci};
    }

  protected:
    // 统一清理 pass 级资源（管线/布局/材质描述符）
    // 注意：set0/set2 全局资源属于 FrameResource，不在这里释放。
    void ClearPassResources()
    {
      pass_resources_.material_ds_info.sets.clear();
      pass_resources_.material_ds_info.pool = nullptr;
      pass_resources_.material_ds_info.layout = nullptr;
      pass_resources_.Colorpipeline = nullptr;
      pass_resources_.Depthpipeline = nullptr;
      pass_resources_.pipeline_layout = nullptr;
    }

  protected:
    // 所有子类共享一份采样器
    static vk::raii::Sampler common_sampler_;
    std::vector<TextureResource> textures_;
    PassResource pass_resources_;
    std::string shader_path_ = "";
    std::string ShadowShader_path_ = "";
    RenderType render_type_ = RenderType::Opaque;

  };

} // namespace vkfw
