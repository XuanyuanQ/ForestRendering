#include "vk/features/skybox/SkyboxPass.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/renderer/FrameContext.hpp"
#include "vk/renderer/FrameGlobals.hpp"
#include "vk/renderer/helper.hpp"
#include "vk/scene/Vertex.hpp"

#include <array>
#include <cstring>
#include <stdexcept>

namespace vkfw
{
  void SkyboxPass::CreateDescriptors(VkContext &ctx, uint32_t image_count)
  {
    auto &device = ctx.Device();
    ubo_ds_info_.layout = CreateSingleBindingDescriptorSetLayout(
        device, 0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
    ubo_ds_info_.pool = CreateSingleTypeDescriptorPool(device, vk::DescriptorType::eUniformBuffer, image_count, image_count);
    ubo_ds_info_.sets = AllocateDescriptorSets(device, ubo_ds_info_.pool, ubo_ds_info_.layout, image_count);

    CreateMappedBuffers(device, ctx.PhysicalDevice(), image_count, sizeof(SkyboxUBO),
                        vk::BufferUsageFlagBits::eUniformBuffer, ubo_buf_, ubo_mem_, ubo_map_);
    for (uint32_t i = 0; i < image_count; ++i)
    {
      WriteUniformBufferDescriptor(device, *ubo_ds_info_.sets[i], 0, *ubo_buf_[i], sizeof(SkyboxUBO));
    }
  }

  void SkyboxPass::CreatePipeline(const vk::raii::Device &device, vk::Format color_format, vk::Format depth_format)
  {
    auto vert_code = ReadFile("res/vk/skybox.vert.spv");
    auto frag_code = ReadFile("res/vk/skybox.frag.spv");

    vk::ShaderModuleCreateInfo vm_ci{};
    vm_ci.codeSize = vert_code.size();
    vm_ci.pCode = reinterpret_cast<uint32_t const *>(vert_code.data());
    vk::raii::ShaderModule vm{device, vm_ci};

    vk::ShaderModuleCreateInfo fm_ci{};
    fm_ci.codeSize = frag_code.size();
    fm_ci.pCode = reinterpret_cast<uint32_t const *>(frag_code.data());
    vk::raii::ShaderModule fm{device, fm_ci};

    vk::PipelineShaderStageCreateInfo stages[2]{};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = *vm;
    stages[0].pName = "main";
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = *fm;
    stages[1].pName = "main";

    std::array<vk::VertexInputBindingDescription, 1> bindings = {
        vk::VertexInputBindingDescription{0u, sizeof(Vertex), vk::VertexInputRate::eVertex}};
    std::array<vk::VertexInputAttributeDescription, 1> attrs = {
        vk::VertexInputAttributeDescription{0u, 0u, vk::Format::eR32G32B32Sfloat, static_cast<uint32_t>(offsetof(Vertex, pos))}};
    vk::PipelineVertexInputStateCreateInfo vi{};
    vi.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
    vi.pVertexBindingDescriptions = bindings.data();
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vi.pVertexAttributeDescriptions = attrs.data();

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
    dss.depthTestEnable = 1;
    dss.depthWriteEnable = 0;
    dss.depthCompareOp = vk::CompareOp::eLessOrEqual;

    vk::PipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                         vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo cb{};
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    std::array<vk::DynamicState, 2> dyn{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dy{};
    dy.dynamicStateCount = static_cast<uint32_t>(dyn.size());
    dy.pDynamicStates = dyn.data();

    vk::PipelineLayoutCreateInfo pl_ci{};
    vk::DescriptorSetLayout raw_dsl = *ubo_ds_info_.layout;
    pl_ci.setLayoutCount = 1;
    pl_ci.pSetLayouts = &raw_dsl;
    pass_resources_.pipeline_layout = vk::raii::PipelineLayout{device, pl_ci};

    vk::PipelineRenderingCreateInfo rci{};
    rci.colorAttachmentCount = 1;
    rci.pColorAttachmentFormats = &color_format;
    if (depth_format != vk::Format::eUndefined)
      rci.depthAttachmentFormat = depth_format;

    vk::GraphicsPipelineCreateInfo gp{};
    gp.pNext = &rci;
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vp_state;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pDepthStencilState = (depth_format != vk::Format::eUndefined) ? &dss : nullptr;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dy;
    gp.layout = *pass_resources_.pipeline_layout;
    pass_resources_.Colorpipeline = vk::raii::Pipeline{device, nullptr, gp};
  }

  bool SkyboxPass::Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets)
  {
    auto &device = ctx.Device();
    CreateDescriptors(ctx, swapchain.ImageCount());
    CreatePipeline(device, swapchain.Format(), targets.shared_depth.Valid() ? targets.shared_depth.format : vk::Format::eUndefined);

    static constexpr Vertex kVerts[] = {
        {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

        {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

        {{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

        {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

        {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

        {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}};

    vk::DeviceSize vb_size = sizeof(kVerts);
    vk::BufferCreateInfo vb_ci{};
    vb_ci.size = vb_size;
    vb_ci.usage = vk::BufferUsageFlagBits::eVertexBuffer;
    vb_ci.sharingMode = vk::SharingMode::eExclusive;
    vertex_buffer_ = vk::raii::Buffer{device, vb_ci};

    auto req = vertex_buffer_.getMemoryRequirements();
    vk::MemoryAllocateInfo vb_mai{};
    vb_mai.allocationSize = req.size;
    vb_mai.memoryTypeIndex = FindMemoryType(ctx.PhysicalDevice(), req.memoryTypeBits,
                                            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    vertex_memory_ = vk::raii::DeviceMemory{device, vb_mai};
    vertex_buffer_.bindMemory(*vertex_memory_, 0);

    void *dst = vertex_memory_.mapMemory(0, vb_size);
    std::memcpy(dst, kVerts, static_cast<size_t>(vb_size));
    vertex_memory_.unmapMemory();

    return true;
  }

  void SkyboxPass::Destroy(VkContext &ctx)
  {
    ctx.Device().waitIdle();

    // Skybox 自有 UBO（当前稳定版本仍保留在 pass 内部）
    UnmapAndClearMappedBuffers(ubo_mem_, ubo_map_);
    ubo_buf_.clear();
    ubo_ds_info_.sets.clear();
    ubo_ds_info_.pool = nullptr;
    ubo_ds_info_.layout = nullptr;
    // 顶点缓冲
    vertex_buffer_ = nullptr;
    vertex_memory_ = nullptr;
    // 重构后 pass_resources_ 清理（管线/布局）
    ClearPassResources();
    IRenderPass::Destroy(ctx);
  }

  void SkyboxPass::OnSwapchainRecreated(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets)
  {
    (void)ctx;
    (void)swapchain;
    (void)targets;
  }

  void SkyboxPass::Record(FrameContext &frame, RenderTargets &targets)
  {
    if (!frame.cmd || !frame.globals)
      return;

    auto &cmd = *frame.cmd;
    uint32_t img = frame.image_index;

    SkyboxUBO ubo{};
    glm::mat4 const view_no_trans = glm::mat4(glm::mat3(frame.globals->view));
    ubo.world_to_clip = frame.globals->proj * view_no_trans;
    ubo.light_position = glm::vec4(frame.globals->light_position, 0.0f);
    ubo.time_seconds = frame.globals->time_seconds;

    if (img < ubo_map_.size() && ubo_map_[img])
      std::memcpy(ubo_map_[img], &ubo, sizeof(ubo));

    vk::ClearValue clear_color{};
    clear_color.color = vk::ClearColorValue(std::array<float, 4>{{0.0f, 0.0f, 0.0f, 1.0f}});
    vk::RenderingAttachmentInfo color_att{};
    color_att.imageView = frame.swapchain_image_view;
    color_att.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_att.loadOp = vk::AttachmentLoadOp::eClear;
    color_att.storeOp = vk::AttachmentStoreOp::eStore;
    color_att.clearValue = clear_color;

    vk::RenderingInfo ri{};
    ri.renderArea.offset = vk::Offset2D{0, 0};
    ri.renderArea.extent = frame.swapchain_extent;
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &color_att;

    vk::RenderingAttachmentInfo depth_att{};
    if (targets.shared_depth.Valid())
    {
      depth_att.imageView = targets.shared_depth.view;
      depth_att.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
      depth_att.loadOp = vk::AttachmentLoadOp::eClear;
      depth_att.storeOp = vk::AttachmentStoreOp::eStore;
      depth_att.clearValue = vk::ClearValue(vk::ClearDepthStencilValue{1.0f, 0});
      ri.pDepthAttachment = &depth_att;
    }

    cmd.beginRendering(ri);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pass_resources_.Colorpipeline);
    cmd.setViewport(0, vk::Viewport{0.0f, 0.0f, static_cast<float>(frame.swapchain_extent.width),
                                    static_cast<float>(frame.swapchain_extent.height), 0.0f, 1.0f});
    cmd.setScissor(0, vk::Rect2D{vk::Offset2D{0, 0}, frame.swapchain_extent});

    cmd.bindVertexBuffers(0, *vertex_buffer_, {0});
    if (img < ubo_ds_info_.sets.size())
    {
      cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pass_resources_.pipeline_layout, 0, {*ubo_ds_info_.sets[img]}, {});
    }
    cmd.draw(36, 1, 0, 0);
    cmd.endRendering();
  }

} // namespace vkfw
