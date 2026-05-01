#include "vk/features/gbuffer/GBufferPass.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/renderer/helper.hpp"

#include <array>

namespace vkfw
{
  namespace
  {
    GBufferAttachmentResource CreateAttachment(
        VkContext &ctx,
        vk::Extent2D extent,
        vk::Format format,
        vk::ImageUsageFlags usage,
        vk::ImageAspectFlags aspect)
    {
      auto &device = ctx.Device();

      GBufferAttachmentResource attachment{};
      attachment.format = format;
      attachment.layout = vk::ImageLayout::eUndefined;

      vk::ImageCreateInfo ici{};
      ici.imageType = vk::ImageType::e2D;
      ici.format = format;
      ici.extent = vk::Extent3D{extent.width, extent.height, 1};
      ici.mipLevels = 1;
      ici.arrayLayers = 1;
      ici.samples = vk::SampleCountFlagBits::e1;
      ici.tiling = vk::ImageTiling::eOptimal;
      ici.usage = usage | vk::ImageUsageFlagBits::eSampled;
      ici.sharingMode = vk::SharingMode::eExclusive;
      ici.initialLayout = vk::ImageLayout::eUndefined;
      attachment.image = vk::raii::Image{device, ici};

      auto req = attachment.image.getMemoryRequirements();
      vk::MemoryAllocateInfo mai{};
      mai.allocationSize = req.size;
      mai.memoryTypeIndex = FindMemoryType(
          ctx.PhysicalDevice(),
          req.memoryTypeBits,
          vk::MemoryPropertyFlagBits::eDeviceLocal);
      attachment.memory = vk::raii::DeviceMemory{device, mai};
      attachment.image.bindMemory(*attachment.memory, 0);

      vk::ImageViewCreateInfo ivci{};
      ivci.image = *attachment.image;
      ivci.viewType = vk::ImageViewType::e2D;
      ivci.format = format;
      ivci.subresourceRange.aspectMask = aspect;
      ivci.subresourceRange.levelCount = 1;
      ivci.subresourceRange.layerCount = 1;
      attachment.view = vk::raii::ImageView{device, ivci};

      return attachment;
    }
  } // namespace

  GBufferResources GBufferPass::CreateGBufferResources(VkContext &ctx, vk::Extent2D extent)
  {
    auto &device = ctx.Device();

    GBufferResources out{};
    out.extent = extent;
    out.diffuse = CreateAttachment(
        ctx, extent, vk::Format::eR8G8B8A8Unorm,
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageAspectFlagBits::eColor);
    out.specular = CreateAttachment(
        ctx, extent, vk::Format::eR8G8B8A8Unorm,
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageAspectFlagBits::eColor);
    out.normal = CreateAttachment(
        ctx, extent, vk::Format::eR16G16B16A16Sfloat,
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageAspectFlagBits::eColor);
    out.depth = CreateAttachment(
        ctx, extent, vk::Format::eD32Sfloat,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::ImageAspectFlagBits::eDepth);

    vk::SamplerCreateInfo sci{};
    sci.magFilter = vk::Filter::eLinear;
    sci.minFilter = vk::Filter::eLinear;
    sci.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    sci.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    sci.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    sci.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sci.maxLod = 1.0f;
    out.sampler = vk::raii::Sampler{device, sci};

    return out;
  }

  void GBufferPass::PublishTargets(RenderTargets &targets) const
  {
    targets.has_gbuffer = true;
    targets.gbuffer.diffuse.format = gbuffer_.diffuse.format;
    targets.gbuffer.diffuse.extent = gbuffer_.extent;
    targets.gbuffer.diffuse.image = static_cast<vk::Image>(gbuffer_.diffuse.image);
    targets.gbuffer.diffuse.view = static_cast<vk::ImageView>(gbuffer_.diffuse.view);
    targets.gbuffer.diffuse.layout = gbuffer_.diffuse.layout;

    targets.gbuffer.specular.format = gbuffer_.specular.format;
    targets.gbuffer.specular.extent = gbuffer_.extent;
    targets.gbuffer.specular.image = static_cast<vk::Image>(gbuffer_.specular.image);
    targets.gbuffer.specular.view = static_cast<vk::ImageView>(gbuffer_.specular.view);
    targets.gbuffer.specular.layout = gbuffer_.specular.layout;

    targets.gbuffer.normal.format = gbuffer_.normal.format;
    targets.gbuffer.normal.extent = gbuffer_.extent;
    targets.gbuffer.normal.image = static_cast<vk::Image>(gbuffer_.normal.image);
    targets.gbuffer.normal.view = static_cast<vk::ImageView>(gbuffer_.normal.view);
    targets.gbuffer.normal.layout = gbuffer_.normal.layout;

    targets.gbuffer.depth.format = gbuffer_.depth.format;
    targets.gbuffer.depth.extent = gbuffer_.extent;
    targets.gbuffer.depth.image = static_cast<vk::Image>(gbuffer_.depth.image);
    targets.gbuffer.depth.view = static_cast<vk::ImageView>(gbuffer_.depth.view);
    targets.gbuffer.depth.layout = gbuffer_.depth.layout;
    targets.gbuffer.sampler = static_cast<vk::Sampler>(gbuffer_.sampler);
  }

  void GBufferPass::ClearTargets(RenderTargets &targets) const
  {
    targets.has_gbuffer = false;
    targets.gbuffer = {};
  }

  bool GBufferPass::Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets)
  {
    gbuffer_ = CreateGBufferResources(ctx, swapchain.Extent());
    PublishTargets(targets);
    return true;
  }

  void GBufferPass::Destroy(VkContext &ctx)
  {
    ctx.Device().waitIdle();
    gbuffer_.sampler = nullptr;
    gbuffer_.depth.view = nullptr;
    gbuffer_.depth.image = nullptr;
    gbuffer_.depth.memory = nullptr;
    gbuffer_.normal.view = nullptr;
    gbuffer_.normal.image = nullptr;
    gbuffer_.normal.memory = nullptr;
    gbuffer_.specular.view = nullptr;
    gbuffer_.specular.image = nullptr;
    gbuffer_.specular.memory = nullptr;
    gbuffer_.diffuse.view = nullptr;
    gbuffer_.diffuse.image = nullptr;
    gbuffer_.diffuse.memory = nullptr;
    gbuffer_.extent = {};
    IRenderPass::Destroy(ctx);
  }

  void GBufferPass::OnSwapchainRecreated(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets)
  {
    ctx.Device().waitIdle();
    gbuffer_ = CreateGBufferResources(ctx, swapchain.Extent());
    PublishTargets(targets);
  }

  void GBufferPass::Record(FrameContext &, RenderTargets &)
  {
  }

  void GBufferPass::Execute(
      FrameContext &frame,
      RenderTargets &targets,
      const std::function<void(vk::raii::CommandBuffer &cmd, const vk::PipelineLayout &layout)> &draw_callback)
  {
    auto &cmd = *frame.cmd;
    // 1. 准备一个容器，用来装所有的屏障
    std::vector<vk::ImageMemoryBarrier2> barriers;
    auto transition_color = [&](GBufferAttachmentResource &attachment)
    {
      vk::AccessFlags2 const src_access =
          attachment.layout == vk::ImageLayout::eShaderReadOnlyOptimal ? vk::AccessFlagBits2::eShaderRead : vk::AccessFlags2{};
      vk::PipelineStageFlags2 const src_stage =
          attachment.layout == vk::ImageLayout::eShaderReadOnlyOptimal ? vk::PipelineStageFlagBits2::eFragmentShader : vk::PipelineStageFlagBits2::eTopOfPipe;
      auto barrier= TransitionImage(*attachment.image,
                      attachment.layout,
                      vk::ImageLayout::eColorAttachmentOptimal,
                      vk::ImageAspectFlagBits::eColor,
                      src_access,
                      vk::AccessFlagBits2::eColorAttachmentWrite,
                      src_stage,
                      vk::PipelineStageFlagBits2::eColorAttachmentOutput);
      barriers.push_back(barrier);            
      attachment.layout = vk::ImageLayout::eColorAttachmentOptimal;
    };

    transition_color(gbuffer_.diffuse);
    transition_color(gbuffer_.specular);
    transition_color(gbuffer_.normal);

    vk::DependencyInfo dep{};
    dep.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    dep.pImageMemoryBarriers = barriers.data();;
    cmd.pipelineBarrier2(dep);

    vk::AccessFlags2 const depth_src_access =
        gbuffer_.depth.layout == vk::ImageLayout::eShaderReadOnlyOptimal ? vk::AccessFlagBits2::eShaderRead : vk::AccessFlags2{};
    vk::PipelineStageFlags2 const depth_src_stage =
        gbuffer_.depth.layout == vk::ImageLayout::eShaderReadOnlyOptimal ? vk::PipelineStageFlagBits2::eFragmentShader : vk::PipelineStageFlagBits2::eTopOfPipe;
    TransitionImage(cmd,
                    *gbuffer_.depth.image,
                    gbuffer_.depth.layout,
                    vk::ImageLayout::eDepthAttachmentOptimal,
                    vk::ImageAspectFlagBits::eDepth,
                    depth_src_access,
                    vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                    depth_src_stage,
                    vk::PipelineStageFlagBits2::eEarlyFragmentTests);
    gbuffer_.depth.layout = vk::ImageLayout::eDepthAttachmentOptimal;
    PublishTargets(targets);

    vk::ClearValue clear_color{};
    clear_color.color = vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f});
    vk::ClearValue clear_depth{};
    clear_depth.depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

    std::array<vk::RenderingAttachmentInfo, 3> color_att{};
    color_att[0].imageView = *gbuffer_.diffuse.view;
    color_att[0].imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_att[0].loadOp = vk::AttachmentLoadOp::eClear;
    color_att[0].storeOp = vk::AttachmentStoreOp::eStore;
    color_att[0].clearValue = clear_color;

    color_att[1].imageView = *gbuffer_.specular.view;
    color_att[1].imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_att[1].loadOp = vk::AttachmentLoadOp::eClear;
    color_att[1].storeOp = vk::AttachmentStoreOp::eStore;
    color_att[1].clearValue = clear_color;

    color_att[2].imageView = *gbuffer_.normal.view;
    color_att[2].imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_att[2].loadOp = vk::AttachmentLoadOp::eClear;
    color_att[2].storeOp = vk::AttachmentStoreOp::eStore;
    color_att[2].clearValue = clear_color;

    vk::RenderingAttachmentInfo depth_att{};
    depth_att.imageView = *gbuffer_.depth.view;
    depth_att.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depth_att.loadOp = vk::AttachmentLoadOp::eClear;
    depth_att.storeOp = vk::AttachmentStoreOp::eStore;
    depth_att.clearValue = clear_depth;

    vk::RenderingInfo ri{};
    ri.renderArea.extent = gbuffer_.extent;
    ri.layerCount = 1;
    ri.colorAttachmentCount = static_cast<uint32_t>(color_att.size());
    ri.pColorAttachments = color_att.data();
    ri.pDepthAttachment = &depth_att;

    cmd.beginRendering(ri);
    cmd.setViewport(0, vk::Viewport{0.0f, 0.0f, static_cast<float>(gbuffer_.extent.width), static_cast<float>(gbuffer_.extent.height), 0.0f, 1.0f});
    cmd.setScissor(0, vk::Rect2D{{0, 0}, gbuffer_.extent});
    draw_callback(cmd, *pass_resources_.pipeline_layout);
    cmd.endRendering();

    barriers.clear();
    auto transition_to_shader_read = [&](GBufferAttachmentResource &attachment, vk::ImageAspectFlags aspect, vk::PipelineStageFlags2 src_stage)
    {
      auto barrier = TransitionImage(*attachment.image,
                      attachment.layout,
                      vk::ImageLayout::eShaderReadOnlyOptimal,
                      aspect,
                      aspect == vk::ImageAspectFlagBits::eDepth ? vk::AccessFlagBits2::eDepthStencilAttachmentWrite : vk::AccessFlagBits2::eColorAttachmentWrite,
                      vk::AccessFlagBits2::eShaderRead,
                      src_stage,
                      vk::PipelineStageFlagBits2::eFragmentShader);
                       barriers.push_back(barrier); 
                      attachment.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    };

    transition_to_shader_read(gbuffer_.diffuse, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    transition_to_shader_read(gbuffer_.specular, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    transition_to_shader_read(gbuffer_.normal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    transition_to_shader_read(gbuffer_.depth, vk::ImageAspectFlagBits::eDepth, vk::PipelineStageFlagBits2::eLateFragmentTests);
    dep.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    dep.pImageMemoryBarriers = barriers.data();;
    cmd.pipelineBarrier2(dep);
    PublishTargets(targets);
  }

} // namespace vkfw
