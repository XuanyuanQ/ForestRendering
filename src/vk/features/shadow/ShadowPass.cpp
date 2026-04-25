#include "vk/features/shadow/ShadowPass.hpp"
#include "vk/core/VkContext.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/renderer/FrameContext.hpp"
#include "vk/renderer/FrameGlobals.hpp"
#include "vk/renderer/RenderTargets.hpp"
#include "vk/renderer/helper.hpp"
#include "vk/scene/Vertex.hpp"

#include <cstring>
#include <fstream>
#include <stdexcept>

namespace vkfw
{

  bool ShadowPass::Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets)
  {
    auto &device = ctx.Device();

    // 1. 创建阴影图资源 (D32_SFLOAT)
    CreateShadowResources(ctx, resolution_);

    targets.has_shadow = true;
    targets.shadow_map.format = vk::Format::eD32Sfloat;
    targets.shadow_map.extent = vk::Extent2D{resolution_, resolution_};
    targets.shadow_map.image = static_cast<vk::Image>(shadow_image_);
    targets.shadow_map.view = static_cast<vk::ImageView>(shadow_view_);
    targets.shadow_map.sampler = static_cast<vk::Sampler>(shadow_sampler_);
    targets.shadow_map.layout = shadow_layout_;

    uint32_t const image_count = swapchain.ImageCount();

    // 2. 创建 UBO 描述符 (Light matrix)
    dsl_ = CreateDescriptorSetLayout(device);
    tex_dsl_ = CreateTextureSetLayout(device);

    dp_ = CreateSingleTypeDescriptorPool(device, vk::DescriptorType::eUniformBuffer, image_count, image_count);
    ds_ = AllocateDescriptorSets(device, dp_, dsl_, image_count);

    CreateMappedBuffers(device, ctx.PhysicalDevice(), image_count, sizeof(CameraUBO), vk::BufferUsageFlagBits::eUniformBuffer, ubo_buf_, ubo_mem_, ubo_map_);
    for (uint32_t i = 0; i < image_count; ++i)
    {
      WriteUniformBufferDescriptor(device, *ds_[i], 0, *ubo_buf_[i], sizeof(CameraUBO));
    }

    pipeline_layout_ = CreatePipelineLayout(device, *dsl_);
    // 2. 创建极简管线 (只有 Vertex Shader, 无 Fragment Shader)
    terrain_shadow_pipeline_ = CreatePipeline(device, vk::Format::eD32Sfloat, "res/vk/shadow_terrarin.spv", "vertMain", "fragMain", false);
    mesh_shadow_pipeline_ = CreatePipeline(device, vk::Format::eD32Sfloat, "res/vk/shadow_mesh.spv", "vertMain", "fragMain", true);

    return true;
  }

  void ShadowPass::Execute(FrameContext &frame,
                           RenderTargets &targets,
                           const std::function<void(vk::raii::CommandBuffer &cmd, const vk::PipelineLayout &layout)> &draw_callback)

  {
    auto &cmd = *frame.cmd;
    uint32_t const img = frame.image_index;

    // 更新当前帧的光源矩阵
    if (img < ubo_map_.size() && ubo_map_[img] && frame.globals)
    {
      CameraUBO ubo{};
      ubo.view = frame.globals->view;
      ubo.proj = frame.globals->proj;
      ubo.model = glm::mat4(1.0f);
      ubo.light = frame.globals->light;
      ubo.camera_pos = glm::vec4(frame.globals->camera_pos, 1.0f);

      glm::vec3 const light_pos = frame.globals->light_position;
      float const len2 = glm::dot(light_pos, light_pos);
      glm::vec3 const dir_to_light = (len2 > 1e-6f) ? glm::normalize(light_pos) : glm::vec3(0.0f, 1.0f, 0.0f);
      ubo.light_dir = glm::vec4(dir_to_light, 0.0f);
      std::memcpy(ubo_map_[img], &ubo, sizeof(ubo));
    }

    // 1. 布局转换：Undefined/ShaderRead -> DepthAttachmentOptimal
    TransitionImage(cmd, *shadow_image_,
                    shadow_layout_,                                    // 旧布局
                    vk::ImageLayout::eDepthAttachmentOptimal,          // 新布局：深度附件
                    vk::ImageAspectFlagBits::eDepth,                   // 只影响深度位
                    {},                                                // 之前没操作，Access 为空
                    vk::AccessFlagBits2::eDepthStencilAttachmentWrite, // 之后要写深度
                    vk::PipelineStageFlagBits2::eTopOfPipe,            // 尽早开始
                    vk::PipelineStageFlagBits2::eEarlyFragmentTests    // 在深度测试前就得转好
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

    // 设置视口为阴影图大小
    cmd.setViewport(0, vk::Viewport{0.0f, 0.0f, (float)resolution_, (float)resolution_, 0.0f, 1.0f});
    cmd.setScissor(0, vk::Rect2D{{0, 0}, {resolution_, resolution_}});

    // cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);

    // 2. 执行外部传入的绘制指令
    // 这个 lambda 会由 App.cpp 提供，内容是：terrain.DrawRaw(cmd) + forest.DrawRaw(cmd)
    if (img < ds_.size())
    {
      cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline_layout_, 0, {*ds_[img]}, {});
    }
    draw_callback(cmd, *pipeline_layout_);

    cmd.endRendering();

    TransitionImage(cmd, *shadow_image_,
                    vk::ImageLayout::eDepthAttachmentOptimal, // 旧布局：深度附件
                    vk::ImageLayout::eShaderReadOnlyOptimal,  // 新布局：着色器只读
                    vk::ImageAspectFlagBits::eDepth,
                    vk::AccessFlagBits2::eDepthStencilAttachmentWrite, // 刚才写完了
                    vk::AccessFlagBits2::eShaderRead,                  // 接下来要读
                    vk::PipelineStageFlagBits2::eLateFragmentTests,    // 必须等深度测试全画完
                    vk::PipelineStageFlagBits2::eFragmentShader        // 在片元着色器采样前转好
    );
    shadow_layout_ = vk::ImageLayout::eShaderReadOnlyOptimal;
    targets.shadow_map.layout = shadow_layout_;
  }

  vk::raii::DescriptorSetLayout ShadowPass::CreateDescriptorSetLayout(const vk::raii::Device &device)
  {
    return CreateSingleBindingDescriptorSetLayout(device, 0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex);
  }

  vk::raii::DescriptorSetLayout ShadowPass::CreateTextureSetLayout(const vk::raii::Device &device)
  {
    return CreateSingleBindingDescriptorSetLayout(device, 0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
  }

  vk::raii::Pipeline ShadowPass::CreatePipeline(const vk::raii::Device &device,
                                                vk::Format depth_format,
                                                const std::string &path,
                                                const char *vert_entry,
                                                const char *frag_entry,
                                                bool instanced)
  {
    // 加载 shadow.vert.spv (只有顶点着色器)
    auto const code = ReadFile(path);
    vk::ShaderModuleCreateInfo sm_ci{.codeSize = code.size(), .pCode = (uint32_t *)code.data()};
    vk::raii::ShaderModule shader_module{device, sm_ci};

    vk::PipelineShaderStageCreateInfo stages[2]{};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = *shader_module;
    stages[0].pName = vert_entry;
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = *shader_module;
    stages[1].pName = frag_entry;

    // 顶点输入：和 Terrain 一模一样，这样可以复用同一个 VBO
    auto binding = VertexBindingDescriptions();
    auto attrs = VertexAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vi{};
    vi.vertexBindingDescriptionCount = static_cast<uint32_t>(binding.size());
    vi.pVertexBindingDescriptions = binding.data();
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vi.pVertexAttributeDescriptions = attrs.data();

    // 3. 输入装配
    vk::PipelineInputAssemblyStateCreateInfo ia{};
    ia.topology = vk::PrimitiveTopology::eTriangleList;

    // 4. 视口与裁剪 (设为动态，方便根据 ShadowMap 大小调整)
    vk::PipelineViewportStateCreateInfo vp{};
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    // 5. 光栅化 (关键：开启 Depth Bias 防止阴影粉刺)
    vk::PipelineRasterizationStateCreateInfo rs{};
    rs.polygonMode = vk::PolygonMode::eFill;
    rs.cullMode = vk::CullModeFlagBits::eBack; // 剔除背面防止自遮挡
    rs.frontFace = vk::FrontFace::eCounterClockwise;
    rs.lineWidth = 1.0f;

    // 强制开启深度偏移
    rs.depthBiasEnable = VK_TRUE;
    rs.depthBiasConstantFactor = 1.25f; // 常数偏移
    rs.depthBiasSlopeFactor = 1.75f;    // 斜率偏移

    // 6. 多重采样 (阴影图通常不开启 MSAA)
    vk::PipelineMultisampleStateCreateInfo ms{};
    ms.rasterizationSamples = vk::SampleCountFlagBits::e1;

    // 7. 深度测试 (必须开启写入)
    vk::PipelineDepthStencilStateCreateInfo dss{};
    dss.depthTestEnable = VK_TRUE;
    dss.depthWriteEnable = VK_TRUE;
    dss.depthCompareOp = vk::CompareOp::eLessOrEqual;

    // 8. 颜色混合 (关键：必须设为 0，因为没有颜色附件)
    vk::PipelineColorBlendStateCreateInfo cb{};
    cb.attachmentCount = 0;
    cb.pAttachments = nullptr;

    // 9. 动态状态
    std::array<vk::DynamicState, 2> dyn = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dyn_ci{};
    dyn_ci.dynamicStateCount = static_cast<uint32_t>(dyn.size());
    dyn_ci.pDynamicStates = dyn.data();

    // 10. 渲染结构定义 (使用动态渲染 Dynamic Rendering)
    vk::PipelineRenderingCreateInfo rendering_ci{.depthAttachmentFormat = depth_format};

    vk::GraphicsPipelineCreateInfo gp_ci{};
    gp_ci.pNext = &rendering_ci; // 关联动态渲染
    gp_ci.stageCount = 2;
    gp_ci.pStages = stages;
    gp_ci.pVertexInputState = &vi;
    gp_ci.pInputAssemblyState = &ia;
    gp_ci.pViewportState = &vp;
    gp_ci.pRasterizationState = &rs;
    gp_ci.pMultisampleState = &ms;
    gp_ci.pDepthStencilState = &dss;
    gp_ci.pColorBlendState = &cb;
    gp_ci.pDynamicState = &dyn_ci;
    gp_ci.layout = *pipeline_layout_; // 包含光源矩阵 UBO 的布局

    return vk::raii::Pipeline{device, nullptr, gp_ci};
  }

  vk::raii::PipelineLayout ShadowPass::CreatePipelineLayout(const vk::raii::Device &device, const vk::DescriptorSetLayout &raw_dsl)
  {
    vk::PipelineLayoutCreateInfo pl_ci{};
    std::array<vk::DescriptorSetLayout, 2> layouts = {raw_dsl, *tex_dsl_};
    pl_ci.setLayoutCount = static_cast<uint32_t>(layouts.size());
    pl_ci.pSetLayouts = layouts.data();
    // 这里绑定包含 LightMatrix 的 DescriptorSetLayout
    return vk::raii::PipelineLayout{device, pl_ci};
  }

  void ShadowPass::CreateShadowResources(VkContext &ctx, uint32_t res)
  {
    auto &device = ctx.Device();

    // 1. 创建阴影图 Image
    // 注意：格式必须是深度格式，Usage 必须包含 DepthAttachment 和 Sampled（以便在主 Pass 读取）
    vk::ImageCreateInfo ici{};
    ici.imageType = vk::ImageType::e2D;
    ici.format = vk::Format::eD32Sfloat; // 或者 eD24UnormS8Uint
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

    // 2. 分配并绑定内存
    auto req = shadow_image_.getMemoryRequirements();
    vk::MemoryAllocateInfo mai{};
    mai.allocationSize = req.size;
    // 阴影图存在 GPU 中，使用 DeviceLocal 性能最高
    mai.memoryTypeIndex = FindMemoryType(
        ctx.PhysicalDevice(),
        req.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    shadow_mem_ = vk::raii::DeviceMemory{device, mai};
    shadow_image_.bindMemory(*shadow_mem_, 0);

    // 3. 创建 ImageView
    // 关键点：subresourceRange.aspectMask 必须是 eDepth
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

    // 4. 创建专用采样器 (Shadow Sampler)
    // 重点：开启 compareEnable，这样 Shader 采样时会返回 0 或 1 (PCF 基础)
    vk::SamplerCreateInfo sci{};
    sci.magFilter = vk::Filter::eLinear;
    sci.minFilter = vk::Filter::eLinear;
    sci.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sci.addressModeU = vk::SamplerAddressMode::eClampToBorder; // 超出光源投影范围 -> 视作不在阴影中
    sci.addressModeV = vk::SamplerAddressMode::eClampToBorder;
    sci.addressModeW = vk::SamplerAddressMode::eClampToBorder;
    sci.mipLodBias = 0.0f;
    sci.maxAnisotropy = 1.0f;
    sci.minLod = 0.0f;
    sci.maxLod = 1.0f;
    sci.borderColor = vk::BorderColor::eFloatOpaqueWhite;

    // 开启硬件深度比较
    // 主渲染里先用手动 compare（更直观也更兼容），所以这里不启用 compare sampler。
    sci.compareEnable = VK_FALSE;
    sci.compareOp = vk::CompareOp::eLessOrEqual;

    shadow_sampler_ = vk::raii::Sampler{device, sci};
  }
}
