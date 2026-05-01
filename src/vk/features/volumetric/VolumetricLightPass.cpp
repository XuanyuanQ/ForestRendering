#include "vk/features/volumetric/VolumetricLightPass.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/renderer/FrameContext.hpp"
#include "vk/renderer/RenderTargets.hpp"

#include <array>

namespace vkfw
{

  bool VolumetricLightPass::Create(VkContext &, VkSwapchain const &, RenderTargets &)
  {
    return true;
  }

  void VolumetricLightPass::SetupPassLayout(VkContext &ctx,
                                            VkSwapchain const &swapchain,
                                            RenderTargets &targets,
                                            FrameResource &frame_resources)
  {
    (void)targets;
    auto &device = ctx.Device();

    // set0: CameraUBO, set1: GBuffer (diff/spec/norm/depth), set2: shadowMap
    vk::PipelineLayoutCreateInfo pl_ci{};
    std::array<vk::DescriptorSetLayout, 3> set_layouts = {
        *frame_resources.ubo_ds_info.layout,
        *frame_resources.gbuffer_ds_info.layout,
        *frame_resources.shadow_ds_info.layout};
    pl_ci.setLayoutCount = static_cast<uint32_t>(set_layouts.size());
    pl_ci.pSetLayouts = set_layouts.data();
    pipeline_layout_ = vk::raii::PipelineLayout{device, pl_ci};

    auto const code = ReadFile("res/vk/volumetric.spv");
    vk::ShaderModuleCreateInfo sm_ci{};
    sm_ci.codeSize = code.size();
    sm_ci.pCode = reinterpret_cast<uint32_t const *>(code.data());
    vk::raii::ShaderModule shader_module{device, sm_ci};

    vk::PipelineShaderStageCreateInfo stages[2]{};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = *shader_module;
    stages[0].pName = "vertMain";
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = *shader_module;
    stages[1].pName = "fragMain";

    vk::PipelineVertexInputStateCreateInfo vi{};
    vk::PipelineInputAssemblyStateCreateInfo ia{};
    ia.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo vp_state{};
    vp_state.viewportCount = 1;
    vp_state.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rs{};
    rs.polygonMode = vk::PolygonMode::eFill;
    rs.cullMode = vk::CullModeFlagBits::eNone;
    rs.frontFace = vk::FrontFace::eClockwise;
    rs.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo ms{};
    ms.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineDepthStencilStateCreateInfo dss{};
    dss.depthTestEnable = VK_FALSE;
    dss.depthWriteEnable = VK_FALSE;

    // Additive blending, scaled by source alpha.
    vk::PipelineColorBlendAttachmentState cb_att{};
    cb_att.blendEnable = VK_TRUE;
    cb_att.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    cb_att.dstColorBlendFactor = vk::BlendFactor::eOne;
    cb_att.colorBlendOp = vk::BlendOp::eAdd;
    cb_att.srcAlphaBlendFactor = vk::BlendFactor::eZero;
    cb_att.dstAlphaBlendFactor = vk::BlendFactor::eOne;
    cb_att.alphaBlendOp = vk::BlendOp::eAdd;
    cb_att.colorWriteMask = vk::ColorComponentFlagBits::eR |
                            vk::ColorComponentFlagBits::eG |
                            vk::ColorComponentFlagBits::eB |
                            vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo cb{};
    cb.attachmentCount = 1;
    cb.pAttachments = &cb_att;

    std::array<vk::DynamicState, 2> dyn{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dyn_ci{};
    dyn_ci.dynamicStateCount = static_cast<uint32_t>(dyn.size());
    dyn_ci.pDynamicStates = dyn.data();

    vk::Format const color_format = swapchain.Format();
    vk::PipelineRenderingCreateInfo rendering_ci{};
    rendering_ci.colorAttachmentCount = 1;
    rendering_ci.pColorAttachmentFormats = &color_format;

    vk::GraphicsPipelineCreateInfo gp_ci{};
    gp_ci.pNext = &rendering_ci;
    gp_ci.stageCount = 2;
    gp_ci.pStages = stages;
    gp_ci.pVertexInputState = &vi;
    gp_ci.pInputAssemblyState = &ia;
    gp_ci.pViewportState = &vp_state;
    gp_ci.pRasterizationState = &rs;
    gp_ci.pMultisampleState = &ms;
    gp_ci.pDepthStencilState = &dss;
    gp_ci.pColorBlendState = &cb;
    gp_ci.pDynamicState = &dyn_ci;
    gp_ci.layout = *pipeline_layout_;
    gp_ci.renderPass = nullptr;

    pipeline_ = vk::raii::Pipeline{device, nullptr, gp_ci};
  }

  void VolumetricLightPass::Destroy(VkContext &ctx)
  {
    ctx.Device().waitIdle();
    debugParameter_ = nullptr;
    pipeline_ = nullptr;
    pipeline_layout_ = nullptr;
    IRenderPass::Destroy(ctx);
  }

  void VolumetricLightPass::OnSwapchainRecreated(VkContext &, VkSwapchain const &, RenderTargets &)
  {
  }

  void VolumetricLightPass::Record(FrameContext &frame, RenderTargets &targets)
  {
    if (frame.cmd == nullptr || frame.resources == nullptr || !targets.gbuffer.Valid())
      return;
    if (debugParameter_ && !debugParameter_->volumtricl)
      return;

    auto &cmd = *frame.cmd;

    // Ensure swapchain image is in color-attachment layout for this pass.
    if (frame.swapchain_old_layout != vk::ImageLayout::eColorAttachmentOptimal)
    {
      TransitionImage(cmd, frame.swapchain_image, frame.swapchain_old_layout, vk::ImageLayout::eColorAttachmentOptimal,
                      vk::ImageAspectFlagBits::eColor, {}, vk::AccessFlagBits2::eColorAttachmentWrite,
                      vk::PipelineStageFlagBits2::eTopOfPipe, vk::PipelineStageFlagBits2::eColorAttachmentOutput);
      frame.swapchain_old_layout = vk::ImageLayout::eColorAttachmentOptimal;
    }

    vk::RenderingAttachmentInfo att{};
    att.imageView = frame.swapchain_image_view;
    att.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    att.loadOp = vk::AttachmentLoadOp::eLoad;
    att.storeOp = vk::AttachmentStoreOp::eStore;

    vk::RenderingInfo ri{};
    ri.renderArea.offset = vk::Offset2D{0, 0};
    ri.renderArea.extent = frame.swapchain_extent;
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &att;

    cmd.beginRendering(ri);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    cmd.setViewport(0, vk::Viewport{0.0f, 0.0f, static_cast<float>(frame.swapchain_extent.width),
                                    static_cast<float>(frame.swapchain_extent.height), 0.0f, 1.0f});
    cmd.setScissor(0, vk::Rect2D{vk::Offset2D{0, 0}, frame.swapchain_extent});

    if (frame.image_index < frame.resources->ubo_ds_info.sets.size() &&
        frame.image_index < frame.resources->gbuffer_ds_info.sets.size() &&
        frame.image_index < frame.resources->shadow_ds_info.sets.size())
    {
      std::array<vk::DescriptorSet, 3> sets = {
          *frame.resources->ubo_ds_info.sets[frame.image_index],
          *frame.resources->gbuffer_ds_info.sets[frame.image_index],
          *frame.resources->shadow_ds_info.sets[frame.image_index]};
      cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline_layout_, 0, sets, {});
    }

    cmd.draw(3, 1, 0, 0);
    cmd.endRendering();
  }

} // namespace vkfw

