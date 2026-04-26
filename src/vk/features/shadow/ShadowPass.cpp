#include "vk/features/shadow/ShadowPass.hpp"
#include "vk/core/VkContext.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/renderer/FrameContext.hpp"
#include "vk/renderer/FrameGlobals.hpp"
#include "vk/renderer/RenderTargets.hpp"
#include "vk/renderer/helper.hpp"

namespace vkfw
{

  bool ShadowPass::Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets)
  {
    auto &device = ctx.Device();

    CreateShadowResources(ctx, resolution_);

    targets.has_shadow = true;
    targets.shadow_map.format = vk::Format::eD32Sfloat;
    targets.shadow_map.extent = vk::Extent2D{resolution_, resolution_};
    targets.shadow_map.image = static_cast<vk::Image>(shadow_image_);
    targets.shadow_map.view = static_cast<vk::ImageView>(shadow_view_);
    targets.shadow_map.sampler = static_cast<vk::Sampler>(shadow_sampler_);
    targets.shadow_map.layout = shadow_layout_;
    (void)swapchain;

    return true;
  }

  void ShadowPass::Execute(FrameContext &frame,
                           RenderTargets &targets,
                           const std::function<void(vk::raii::CommandBuffer &cmd, const vk::PipelineLayout &layout)> &draw_callback)

  {
    auto &cmd = *frame.cmd;
    uint32_t const img = frame.image_index;

    TransitionImage(cmd, *shadow_image_,
                    shadow_layout_,
                    vk::ImageLayout::eDepthAttachmentOptimal,
                    vk::ImageAspectFlagBits::eDepth,
                    {},
                    vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                    vk::PipelineStageFlagBits2::eTopOfPipe,
                    vk::PipelineStageFlagBits2::eEarlyFragmentTests
    );
    shadow_layout_ = vk::ImageLayout::eDepthAttachmentOptimal;
    targets.shadow_map.layout = shadow_layout_;
    vk::RenderingAttachmentInfo depth_att{};
    depth_att.imageView = *shadow_view_;
    depth_att.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depth_att.loadOp = vk::AttachmentLoadOp::eClear;
    depth_att.storeOp = vk::AttachmentStoreOp::eStore;
    depth_att.clearValue.depthStencil = {1.0f, 0};

    vk::RenderingInfo ri{};
    ri.renderArea.extent = vk::Extent2D{resolution_, resolution_};
    ri.layerCount = 1;
    ri.pDepthAttachment = &depth_att;

    cmd.beginRendering(ri);

    cmd.setViewport(0, vk::Viewport{0.0f, 0.0f, (float)resolution_, (float)resolution_, 0.0f, 1.0f});
    cmd.setScissor(0, vk::Rect2D{{0, 0}, {resolution_, resolution_}});
    draw_callback(cmd, *pass_resources_.pipeline_layout);

    cmd.endRendering();

    TransitionImage(cmd, *shadow_image_,
                    vk::ImageLayout::eDepthAttachmentOptimal,
                    vk::ImageLayout::eShaderReadOnlyOptimal,
                    vk::ImageAspectFlagBits::eDepth,
                    vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                    vk::AccessFlagBits2::eShaderRead,
                    vk::PipelineStageFlagBits2::eLateFragmentTests,
                    vk::PipelineStageFlagBits2::eFragmentShader
    );
    shadow_layout_ = vk::ImageLayout::eShaderReadOnlyOptimal;
    targets.shadow_map.layout = shadow_layout_;
  }

  void ShadowPass::CreateShadowResources(VkContext &ctx, uint32_t res)
  {
    auto &device = ctx.Device();

    vk::ImageCreateInfo ici{};
    ici.imageType = vk::ImageType::e2D;
    ici.format = vk::Format::eD32Sfloat;
    ici.extent = vk::Extent3D{res, res, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = vk::SampleCountFlagBits::e1;
    ici.tiling = vk::ImageTiling::eOptimal;
    ici.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
    ici.sharingMode = vk::SharingMode::eExclusive;
    ici.initialLayout = vk::ImageLayout::eUndefined;

    shadow_image_ = vk::raii::Image{device, ici};
    shadow_layout_ = vk::ImageLayout::eUndefined;

    auto req = shadow_image_.getMemoryRequirements();
    vk::MemoryAllocateInfo mai{};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemoryType(
        ctx.PhysicalDevice(),
        req.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    shadow_mem_ = vk::raii::DeviceMemory{device, mai};
    shadow_image_.bindMemory(*shadow_mem_, 0);

    vk::ImageViewCreateInfo ivci{};
    ivci.image = *shadow_image_;
    ivci.viewType = vk::ImageViewType::e2D;
    ivci.format = vk::Format::eD32Sfloat;
    ivci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    ivci.subresourceRange.baseMipLevel = 0;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.layerCount = 1;

    shadow_view_ = vk::raii::ImageView{device, ivci};

    vk::SamplerCreateInfo sci{};
    sci.magFilter = vk::Filter::eLinear;
    sci.minFilter = vk::Filter::eLinear;
    sci.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sci.addressModeU = vk::SamplerAddressMode::eClampToBorder;
    sci.addressModeV = vk::SamplerAddressMode::eClampToBorder;
    sci.addressModeW = vk::SamplerAddressMode::eClampToBorder;
    sci.mipLodBias = 0.0f;
    sci.maxAnisotropy = 1.0f;
    sci.minLod = 0.0f;
    sci.maxLod = 1.0f;
    sci.borderColor = vk::BorderColor::eFloatOpaqueWhite;

    sci.compareEnable = VK_FALSE;
    sci.compareOp = vk::CompareOp::eLessOrEqual;

    shadow_sampler_ = vk::raii::Sampler{device, sci};
  }
}
