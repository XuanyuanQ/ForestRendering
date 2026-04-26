#include "vk/renderer/VkRenderer.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkFrameSync.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/features/shadow/ShadowPass.hpp"
#include "vk/renderer/IRenderPass.hpp"
#include "vk/renderer/helper.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

namespace vkfw
{
  namespace
  {
    constexpr std::array<RenderType, 6> kPassOrder = {
        RenderType::Shadow,
        RenderType::Skybox,
        RenderType::Opaque,
        RenderType::Lighting,
        RenderType::Transparent,
        RenderType::UI};
  }

  void VkRenderer::AddPass(std::unique_ptr<IRenderPass> pass)
  {
    pass_nodes_.push_back(std::move(pass));
  }

  void VkRenderer::AddObjectPass(std::unique_ptr<IRenderPass> pass)
  {
    pass_nodes_.push_back(std::move(pass));
  }

  void VkRenderer::setShowDepthPass(std::unique_ptr<ShadowPass> pass)
  {
    pass_nodes_.push_back(std::move(pass));
  }

  void VkRenderer::SyncSharedDepthTargets() noexcept
  {
    targets_.shared_depth.format = shared_depth_format_;
    targets_.shared_depth.extent = shared_depth_extent_;
    targets_.shared_depth.layout = shared_depth_layout_;
    targets_.shared_depth.image = static_cast<vk::Image>(shared_depth_img_);
    targets_.shared_depth.view = static_cast<vk::ImageView>(shared_depth_view_);
  }

  void VkRenderer::CreateFrameResources(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets)
  {
    (void)targets;
    auto &device = ctx.Device();
    uint32_t const image_count = swapchain.ImageCount();

    frame_resources_.ubo_ds_info.layout = CreateSingleBindingDescriptorSetLayout(
        device, 0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
    frame_resources_.ubo_ds_info.pool = CreateSingleTypeDescriptorPool(device, vk::DescriptorType::eUniformBuffer, image_count, image_count);
    frame_resources_.ubo_ds_info.sets = AllocateDescriptorSets(device, frame_resources_.ubo_ds_info.pool, frame_resources_.ubo_ds_info.layout, image_count);

    frame_resources_.shadow_ds_info.layout = CreateSingleBindingDescriptorSetLayout(
        device, 0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    frame_resources_.shadow_ds_info.pool = CreateSingleTypeDescriptorPool(device, vk::DescriptorType::eCombinedImageSampler, image_count, image_count);
    frame_resources_.shadow_ds_info.sets = AllocateDescriptorSets(device, frame_resources_.shadow_ds_info.pool, frame_resources_.shadow_ds_info.layout, image_count);

    CreateMappedBuffers(ctx.Device(), ctx.PhysicalDevice(), image_count, sizeof(CameraUBO),
                        vk::BufferUsageFlagBits::eUniformBuffer,
                        frame_resources_.ubo_buf, frame_resources_.ubo_mem, frame_resources_.ubo_map);
    for (uint32_t i = 0; i < image_count; ++i)
    {
      WriteUniformBufferDescriptor(device, *frame_resources_.ubo_ds_info.sets[i], 0, *frame_resources_.ubo_buf[i], sizeof(CameraUBO));
    }
  }

  void VkRenderer::RefreshFrameShadowDescriptors(VkContext &ctx, VkSwapchain const &swapchain)
  {
    if (!targets_.shadow_map.Valid() || frame_resources_.shadow_ds_info.sets.empty())
      return;

    auto &device = ctx.Device();
    uint32_t const image_count = swapchain.ImageCount();
    uint32_t const update_count = std::min(image_count, static_cast<uint32_t>(frame_resources_.shadow_ds_info.sets.size()));
    for (uint32_t i = 0; i < update_count; ++i)
    {
      WriteCombinedImageSamplerDescriptor(device, *frame_resources_.shadow_ds_info.sets[i], 0,
                                          targets_.shadow_map.sampler, targets_.shadow_map.view);
    }
  }

  void VkRenderer::CreateCommandResources(VkContext &ctx, VkFrameSync &sync)
  {
    auto &device = ctx.Device();

    vk::CommandPoolCreateInfo cpci{};
    cpci.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    cpci.queueFamilyIndex = ctx.GraphicsQueueFamilyIndex();
    command_pool_ = vk::raii::CommandPool{device, cpci};

    vk::CommandBufferAllocateInfo cbai{};
    cbai.commandPool = *command_pool_;
    cbai.level = vk::CommandBufferLevel::ePrimary;
    cbai.commandBufferCount = sync.FramesInFlight();
    command_buffers_ = device.allocateCommandBuffers(cbai);
  }

  void VkRenderer::CreateSharedDepth(VkContext &ctx, vk::Extent2D extent)
  {
    DestroySharedDepth(ctx);

    shared_depth_extent_ = extent;
    shared_depth_format_ = FindDepthFormat(ctx.PhysicalDevice());
    shared_depth_layout_ = vk::ImageLayout::eUndefined;

    auto &device = ctx.Device();
    vk::ImageCreateInfo ici{};
    ici.imageType = vk::ImageType::e2D;
    ici.format = shared_depth_format_;
    ici.extent = vk::Extent3D{extent.width, extent.height, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = vk::SampleCountFlagBits::e1;
    ici.tiling = vk::ImageTiling::eOptimal;
    ici.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    ici.sharingMode = vk::SharingMode::eExclusive;
    ici.initialLayout = vk::ImageLayout::eUndefined;
    shared_depth_img_ = vk::raii::Image{device, ici};

    auto req = shared_depth_img_.getMemoryRequirements();
    vk::MemoryAllocateInfo mai{};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemoryType(ctx.PhysicalDevice(), req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    shared_depth_mem_ = vk::raii::DeviceMemory{device, mai};
    shared_depth_img_.bindMemory(*shared_depth_mem_, 0);

    vk::ImageViewCreateInfo ivci{};
    ivci.image = *shared_depth_img_;
    ivci.viewType = vk::ImageViewType::e2D;
    ivci.format = shared_depth_format_;
    ivci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.layerCount = 1;
    shared_depth_view_ = vk::raii::ImageView{device, ivci};

    SyncSharedDepthTargets();
  }

  void VkRenderer::DestroySharedDepth(VkContext &ctx)
  {
    ctx.Device().waitIdle();
    shared_depth_view_ = nullptr;
    shared_depth_img_ = nullptr;
    shared_depth_mem_ = nullptr;
    shared_depth_extent_ = {};
    shared_depth_format_ = vk::Format::eUndefined;
    shared_depth_layout_ = vk::ImageLayout::eUndefined;
    SyncSharedDepthTargets();
  }

  void VkRenderer::DestroyCommandResources(VkContext &ctx)
  {
    ctx.Device().waitIdle();
    command_buffers_.clear();
    command_pool_ = nullptr;
  }

  bool VkRenderer::Create(VkContext &ctx, VkSwapchain &swapchain, VkFrameSync &sync, DebugParam &param)
  {
    sync.EnsureRenderFinishedSize(ctx, swapchain.ImageCount());
    CreateCommandResources(ctx, sync);
    CreateSharedDepth(ctx, swapchain.Extent());

    ForEachPassByType(RenderType::Shadow, [&](IRenderPass &pass) {
      pass.Create(ctx, swapchain, targets_);
    });

    CreateFrameResources(ctx, swapchain, targets_);

    for (auto type : kPassOrder)
    {
      ForEachPassByType(type, [&](IRenderPass &pass) {
        if (type == RenderType::Shadow)
        {
          pass.setDebugParameter(param);
          pass.SetupPassLayout(ctx, swapchain, targets_, frame_resources_);
          return;
        }
        if (!pass.Create(ctx, swapchain, targets_))
          throw std::runtime_error("vkfw::VkRenderer::Create pass create failed");
        pass.setDebugParameter(param);
        pass.SetupPassLayout(ctx, swapchain, targets_, frame_resources_);
      });
    }

    RefreshFrameShadowDescriptors(ctx, swapchain);
    return true;
  }

  void VkRenderer::Destroy(VkContext &ctx)
  {
    for (auto &pass : pass_nodes_)
      pass->Destroy(ctx);
    pass_nodes_.clear();

    // 清理 FrameResource 全局 UBO 与描述符（set0/set2）
    UnmapAndClearMappedBuffers(frame_resources_.ubo_mem, frame_resources_.ubo_map);
    frame_resources_.ubo_buf.clear();
    frame_resources_.ubo_ds_info.sets.clear();
    frame_resources_.ubo_ds_info.pool = nullptr;
    frame_resources_.ubo_ds_info.layout = nullptr;
    frame_resources_.shadow_ds_info.sets.clear();
    frame_resources_.shadow_ds_info.pool = nullptr;
    frame_resources_.shadow_ds_info.layout = nullptr;

    DestroySharedDepth(ctx);
    DestroyCommandResources(ctx);
  }

  void VkRenderer::OnSwapchainRecreated(VkContext &ctx, VkSwapchain &swapchain, VkFrameSync &sync)
  {
    sync.EnsureRenderFinishedSize(ctx, swapchain.ImageCount());
    CreateSharedDepth(ctx, swapchain.Extent());
    for (auto &pass : pass_nodes_)
      pass->OnSwapchainRecreated(ctx, swapchain, targets_);
    RefreshFrameShadowDescriptors(ctx, swapchain);
  }

  bool VkRenderer::DrawFrame(VkContext &ctx, VkSwapchain &swapchain, VkFrameSync &sync, FrameGlobals const &globals)
  {
    sync.WaitForFrame(ctx, frame_index_);
    auto [acq_result, image_index] = swapchain.AcquireNextImage(UINT64_MAX, sync.ImageAvailable(frame_index_), vk::Fence{});
    if (acq_result == vk::Result::eErrorOutOfDateKHR)
      return false;
    if (acq_result != vk::Result::eSuccess && acq_result != vk::Result::eSuboptimalKHR)
      throw std::runtime_error("acquireNextImage failed");

    sync.ResetFence(ctx, frame_index_);
    auto &cmd = command_buffers_.at(frame_index_);
    cmd.reset();
    cmd.begin(vk::CommandBufferBeginInfo{});

    vk::ImageLayout const initial_layout = swapchain.IsFirstUse(image_index) ? vk::ImageLayout::eUndefined : vk::ImageLayout::ePresentSrcKHR;
    TransitionImage(cmd, swapchain.Image(image_index), initial_layout, vk::ImageLayout::eColorAttachmentOptimal,
                    vk::ImageAspectFlagBits::eColor, {}, vk::AccessFlagBits2::eColorAttachmentWrite,
                    vk::PipelineStageFlagBits2::eTopOfPipe, vk::PipelineStageFlagBits2::eColorAttachmentOutput);

    vk::Image const raw_depth_img = static_cast<vk::Image>(shared_depth_img_);
    if (static_cast<VkImage>(raw_depth_img) != VK_NULL_HANDLE && shared_depth_layout_ != vk::ImageLayout::eDepthAttachmentOptimal)
    {
      TransitionImage(cmd, raw_depth_img, shared_depth_layout_, vk::ImageLayout::eDepthAttachmentOptimal,
                      vk::ImageAspectFlagBits::eDepth, {}, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                      vk::PipelineStageFlagBits2::eTopOfPipe, vk::PipelineStageFlagBits2::eEarlyFragmentTests);
      shared_depth_layout_ = vk::ImageLayout::eDepthAttachmentOptimal;
      SyncSharedDepthTargets();
    }

    FrameContext frame{};
    frame.cmd = &cmd;
    frame.frame_index = frame_index_;
    frame.image_index = image_index;
    frame.swapchain_extent = swapchain.Extent();
    frame.swapchain_format = swapchain.Format();
    frame.swapchain_image = swapchain.Image(image_index);
    frame.swapchain_image_view = swapchain.ImageView(image_index);
    frame.swapchain_old_layout = vk::ImageLayout::eColorAttachmentOptimal;
    frame.globals = &globals;
    frame.resources = &frame_resources_;

    ForEachPassByType(RenderType::Shadow, [&](IRenderPass &pass) {
      auto *shadow_pass = dynamic_cast<ShadowPass *>(&pass);
      if (!shadow_pass)
        return;
      shadow_pass->Execute(frame, targets_, [&](vk::raii::CommandBuffer &shadow_cmd, vk::PipelineLayout shadow_layout) {
        ForEachPassByType(RenderType::Opaque, [&](IRenderPass &opaque) {
          if (!opaque.CastsShadow())
            return;
          auto const &depth_pipeline = opaque.GetPassResource().Depthpipeline;
          if (*depth_pipeline == VK_NULL_HANDLE)
            return;
          shadow_cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *depth_pipeline);
          opaque.RecordShadow(frame, shadow_cmd, shadow_layout, frame.image_index);
        });
      });
    });

    for (auto type : kPassOrder)
    {
      if (type == RenderType::Shadow)
        continue;
      ForEachPassByType(type, [&](IRenderPass &pass) {
        pass.Record(frame, targets_);
      });
    }

    if (frame.swapchain_old_layout != vk::ImageLayout::ePresentSrcKHR)
    {
      TransitionImage(cmd, frame.swapchain_image, frame.swapchain_old_layout, vk::ImageLayout::ePresentSrcKHR,
                      vk::ImageAspectFlagBits::eColor, vk::AccessFlagBits2::eColorAttachmentWrite, {},
                      vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe);
      frame.swapchain_old_layout = vk::ImageLayout::ePresentSrcKHR;
    }

    cmd.end();
    swapchain.MarkUsed(image_index);

    vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::SubmitInfo submit{};
    submit.waitSemaphoreCount = 1;
    auto image_avail = sync.ImageAvailable(frame_index_);
    submit.pWaitSemaphores = &image_avail;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    auto raw_cmd = *cmd;
    submit.pCommandBuffers = &raw_cmd;
    submit.signalSemaphoreCount = 1;
    auto render_finished = sync.RenderFinished(image_index);
    submit.pSignalSemaphores = &render_finished;
    ctx.GraphicsQueue().submit(submit, sync.InFlightFence(frame_index_));

    vk::PresentInfoKHR present{};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &render_finished;
    present.swapchainCount = 1;
    auto sc = swapchain.Handle();
    present.pSwapchains = &sc;
    present.pImageIndices = &image_index;
    auto pres_result = ctx.GraphicsQueue().presentKHR(present);
    if (pres_result == vk::Result::eErrorOutOfDateKHR || pres_result == vk::Result::eSuboptimalKHR)
      return false;
    if (pres_result != vk::Result::eSuccess)
      throw std::runtime_error("presentKHR failed");

    frame_index_ = (frame_index_ + 1) % sync.FramesInFlight();
    return true;
  }
} // namespace vkfw
