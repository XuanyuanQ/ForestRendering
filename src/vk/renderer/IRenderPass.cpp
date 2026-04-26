#include "vk/renderer/helper.hpp"
#include "vk/renderer/IRenderPass.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/scene/Vertex.hpp"
#include "vk/core/VkContext.hpp"
#include "config.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif
namespace vkfw
{
  vk::raii::Sampler IRenderPass::common_sampler_{nullptr};
  namespace
  {

    static std::string GetCwd()
    {
#if defined(_WIN32)
      char buf[4096]{};
      if (_getcwd(buf, sizeof(buf)) != nullptr)
        return std::string(buf);
      return std::string("<unknown>");
#else
      char buf[4096]{};
      if (getcwd(buf, sizeof(buf)) != nullptr)
        return std::string(buf);
      return std::string("<unknown>");
#endif
    }

    static bool FileExists(std::string const &path)
    {
      std::ifstream f(path.c_str(), std::ios::binary);
      return f.good();
    }

    static bool StartsWith(std::string const &s, std::string const &prefix)
    {
      return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
    }

    static std::string RootDirFromConfig()
    {
      // `config::resources_path()` picks "." when the requested file exists in CWD.
      // We want the baked-in project root regardless of CWD, so we ask for a (likely)
      // missing file and strip the known suffix.
      static std::string cached = []()
      {
        std::string const marker = "__codex_missing_resource_marker__";
        std::string const dummy = config::resources_path(marker); // "<root>/res/<marker>"

        std::string const suffix = std::string("/res/") + marker;
        if (dummy.size() >= suffix.size() &&
            dummy.compare(dummy.size() - suffix.size(), suffix.size(), suffix) == 0)
        {
          return dummy.substr(0, dummy.size() - suffix.size());
        }

        // Fallback: best-effort strip at the last "/res/".
        auto const pos = dummy.rfind("/res/");
        if (pos != std::string::npos)
          return dummy.substr(0, pos);
        return std::string(".");
      }();
      return cached;
    }

    static std::vector<std::string> BuildTextureCandidates(std::string const &path)
    {
      std::vector<std::string> candidates;
      candidates.reserve(10);

      auto const root = RootDirFromConfig();

      // 1) Original path as provided by the caller.
      candidates.push_back(path);

      // 2) If it looks like "res/...", force project-root version regardless of CWD.
      if (StartsWith(path, "res/"))
      {
        candidates.push_back(root + "/" + path);
        // Also try config helper (may still resolve to "." depending on CWD, but cheap).
        candidates.push_back(config::resources_path(path.substr(4)));
      }
      else
      {
        // Treat it as a resource-relative path.
        candidates.push_back(config::resources_path(path));
        candidates.push_back(root + "/res/" + path);
      }

      // 3) Common when launching from nested build folders.
      candidates.push_back(std::string("../") + path);
      candidates.push_back(std::string("../../") + path);
      candidates.push_back(std::string("../../../") + path);

      // Deduplicate while preserving order.
      std::vector<std::string> uniq;
      uniq.reserve(candidates.size());
      for (auto const &p : candidates)
      {
        if (std::find(uniq.begin(), uniq.end(), p) == uniq.end())
          uniq.push_back(p);
      }
      return uniq;
    }
  } // namespace

  TextureResource IRenderPass::LoadTextureResource(
      vk::raii::Device &device,
      vk::raii::PhysicalDevice &physDevice,
      vk::raii::Queue &queue,
      const std::string &path)
  {
    // 1. Load pixels
    int w, h, c;
    stbi_uc *pixels = nullptr;
    std::string chosen;
    auto const candidates = BuildTextureCandidates(path);
    for (auto const &p : candidates)
    {
      if (!FileExists(p))
        continue;
      pixels = stbi_load(p.c_str(), &w, &h, &c, STBI_rgb_alpha);
      if (pixels != nullptr)
      {
        chosen = p;
        break;
      }
    }
    if (!pixels)
    {
      std::ostringstream oss;
      oss << "Failed to load texture: " << path;
      oss << " (cwd=" << GetCwd() << ")";
      oss << " (exists=" << (FileExists(path) ? 1 : 0) << ")";
      if (auto const *reason = stbi_failure_reason())
        oss << " (stb_reason=" << reason << ")";
      oss << " (tried=";
      for (size_t i = 0; i < candidates.size(); ++i)
      {
        if (i)
          oss << ", ";
        oss << candidates[i];
      }
      oss << ")";
      throw std::runtime_error(oss.str());
    }
    vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(w) * h * 4;

    // 2. Staging Buffer
    vk::BufferCreateInfo staging_ci{};
    staging_ci.size = imageSize;
    staging_ci.usage = vk::BufferUsageFlagBits::eTransferSrc;
    vk::raii::Buffer stagingBuffer{device, staging_ci};

    auto staging_req = stagingBuffer.getMemoryRequirements();

    // 修复：显式赋值 MemoryAllocateInfo
    vk::MemoryAllocateInfo staging_mai{};
    staging_mai.sType = vk::StructureType::eMemoryAllocateInfo;
    staging_mai.allocationSize = staging_req.size;
    staging_mai.memoryTypeIndex = vkfw::FindMemoryType(physDevice, staging_req.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    vk::raii::DeviceMemory stagingMemory{device, staging_mai};
    stagingBuffer.bindMemory(*stagingMemory, 0);

    void *data = stagingMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    stagingMemory.unmapMemory();
    stbi_image_free(pixels);

    // 3. GPU Image
    vk::ImageCreateInfo image_ci{};
    image_ci.imageType = vk::ImageType::e2D;
    image_ci.format = vk::Format::eR8G8B8A8Srgb;
    image_ci.extent = vk::Extent3D{static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    image_ci.mipLevels = 1;
    image_ci.arrayLayers = 1;
    image_ci.samples = vk::SampleCountFlagBits::e1;
    image_ci.tiling = vk::ImageTiling::eOptimal;
    image_ci.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    image_ci.initialLayout = vk::ImageLayout::eUndefined;

    auto image = vk::raii::Image{device, image_ci};

    auto img_req = image.getMemoryRequirements();

    // 修复：显式赋值 MemoryAllocateInfo
    vk::MemoryAllocateInfo img_mai{};
    img_mai.sType = vk::StructureType::eMemoryAllocateInfo;
    img_mai.allocationSize = img_req.size;
    img_mai.memoryTypeIndex = vkfw::FindMemoryType(physDevice, img_req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

    auto memory = vk::raii::DeviceMemory{device, img_mai};
    image.bindMemory(*memory, 0);

    {
      // 修复：CommandPoolCreateInfo 显式赋值
      vk::CommandPoolCreateInfo pool_ci{};
      pool_ci.sType = vk::StructureType::eCommandPoolCreateInfo;
      pool_ci.flags = vk::CommandPoolCreateFlagBits::eTransient;
      pool_ci.queueFamilyIndex = 0;
      vk::raii::CommandPool temp_pool{device, pool_ci};

      // 修复：CommandBufferAllocateInfo 显式赋值
      vk::CommandBufferAllocateInfo cmd_ai{};
      cmd_ai.sType = vk::StructureType::eCommandBufferAllocateInfo;
      cmd_ai.commandPool = *temp_pool;
      cmd_ai.level = vk::CommandBufferLevel::ePrimary;
      cmd_ai.commandBufferCount = 1;

      // 修复：CommandBuffers 构造
      vk::raii::CommandBuffers temp_cmds{device, cmd_ai};
      vk::raii::CommandBuffer cmd = std::move(temp_cmds[0]);

      // 修复：CommandBufferBeginInfo 显式赋值
      vk::CommandBufferBeginInfo begin_info{};
      begin_info.sType = vk::StructureType::eCommandBufferBeginInfo;
      begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
      cmd.begin(begin_info);

      vkfw::TransitionImage(cmd, *image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                            vk::ImageAspectFlagBits::eColor, {}, vk::AccessFlagBits2::eTransferWrite,
                            vk::PipelineStageFlagBits2::eTopOfPipe, vk::PipelineStageFlagBits2::eTransfer);

      vk::BufferImageCopy copy_region{};
      copy_region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
      copy_region.imageSubresource.layerCount = 1;
      copy_region.imageExtent = image_ci.extent;
      cmd.copyBufferToImage(*stagingBuffer, *image, vk::ImageLayout::eTransferDstOptimal, copy_region);

      vkfw::TransitionImage(cmd, *image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                            vk::ImageAspectFlagBits::eColor, vk::AccessFlagBits2::eTransferWrite, vk::AccessFlagBits2::eShaderRead,
                            vk::PipelineStageFlagBits2::eTransfer, vk::PipelineStageFlagBits2::eFragmentShader);
      cmd.end();

      vk::SubmitInfo si{};
      si.sType = vk::StructureType::eSubmitInfo;
      si.commandBufferCount = 1;
      si.pCommandBuffers = &(*cmd);
      device.getQueue(0, 0).submit(si);
      device.getQueue(0, 0).waitIdle();
    }

    // 5. Create View
    vk::ImageViewCreateInfo view_ci{};
    view_ci.sType = vk::StructureType::eImageViewCreateInfo;
    view_ci.image = *image;
    view_ci.viewType = vk::ImageViewType::e2D;
    view_ci.format = vk::Format::eR8G8B8A8Srgb;
    view_ci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    view_ci.subresourceRange.levelCount = 1;
    view_ci.subresourceRange.layerCount = 1;
    auto view = vk::raii::ImageView{device, view_ci};
    return {std::move(image), std::move(memory), std::move(view)};
  }

  void IRenderPass::LoadTexture(VkContext &ctx, const std::string &path, uint32_t index)
  {
    // 1. 扩容
    if (index >= textures_.size())
    {
      textures_.resize(index + 1);
    }
    textures_[index] = LoadTextureResource(
        ctx.Device(),
        ctx.PhysicalDevice(),
        ctx.GraphicsQueue(),
        path);
  }
  std::vector<char> IRenderPass::ReadFile(std::string const &filename)
  {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
      std::cerr << "Failed to open file: " << filename << std::endl;
      return {};
      // throw std::runtime_error("Failed to open file: " + filename);
    }
    std::vector<char> buffer(static_cast<size_t>(file.tellg()));
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return buffer;
  }

  void IRenderPass::SetupPassLayout(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets,FrameResource &frame_resources)
  {
    auto &device = ctx.Device();
    uint32_t const image_count = swapchain.ImageCount();

    bool const is_shadow_pass = (render_type_ == RenderType::Shadow);
    bool const has_material_textures = !textures_.empty();
    bool const needs_material_layout = has_material_textures || is_shadow_pass;

    if (needs_material_layout)
    {
      pass_resources_.material_ds_info.layout = CreateSingleBindingDescriptorSetLayout(
          device, 0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);

      if (has_material_textures)
      {
        uint32_t const material_count = static_cast<uint32_t>(textures_.size());
        uint32_t const set_count = image_count * material_count;
        pass_resources_.material_ds_info.pool = CreateSingleTypeDescriptorPool(
            device, vk::DescriptorType::eCombinedImageSampler, set_count, set_count);
        pass_resources_.material_ds_info.sets = AllocateDescriptorSets(
            device, pass_resources_.material_ds_info.pool, pass_resources_.material_ds_info.layout, set_count);
        for (uint32_t i = 0; i < image_count; ++i)
        {
          for (uint32_t m = 0; m < material_count; ++m)
          {
            uint32_t const idx = i * material_count + m;
            WriteCombinedImageSamplerDescriptor(
                device, *pass_resources_.material_ds_info.sets[idx], 0, *common_sampler_, *textures_[m].view);
          }
        }
      }
    }

    if (!needs_material_layout)
    {
      if (render_type_ == RenderType::Skybox)
      {
        vk::PipelineLayoutCreateInfo skybox_pl_ci{};
        vk::DescriptorSetLayout skybox_set0 = *frame_resources.ubo_ds_info.layout;
        skybox_pl_ci.setLayoutCount = 1;
        skybox_pl_ci.pSetLayouts = &skybox_set0;
        pass_resources_.pipeline_layout = vk::raii::PipelineLayout{device, skybox_pl_ci};
      }
      return;
    }

    vk::PipelineLayoutCreateInfo pl_ci{};
    std::array<vk::DescriptorSetLayout, 3> set_layouts = {
        *frame_resources.ubo_ds_info.layout,
        *pass_resources_.material_ds_info.layout,
        *frame_resources.shadow_ds_info.layout};
    pl_ci.setLayoutCount = static_cast<uint32_t>(set_layouts.size());
    pl_ci.pSetLayouts = set_layouts.data();
    pass_resources_.pipeline_layout = vk::raii::PipelineLayout{device, pl_ci};

    if (shader_path_ == "")
    {
      return;
    }
    pass_resources_.Colorpipeline = CreateColorPipeline(device, shader_path_, swapchain.Format(), targets.shared_depth.format);
    if (ShadowShader_path_ == "")
    {
      return;
    }
    pass_resources_.Depthpipeline = CreateDepthPipeline(device, ShadowShader_path_, swapchain.Format(), vk::Format::eD32Sfloat);
  }

  vk::raii::Pipeline IRenderPass::CreateColorPipeline(
      const vk::raii::Device &device,
      const std::string &shader_path,
      vk::Format color_format,
      vk::Format depth_format)
  {
    // 1. 加载并创建着色器模块
    // 注意：shader_module 是局部变量，但在函数结束前管线已经创建完成，所以没问题
    auto const code = ReadFile(shader_path);
    vk::ShaderModuleCreateInfo sm_ci{};
    sm_ci.codeSize = code.size();
    sm_ci.pCode = reinterpret_cast<uint32_t const *>(code.data());
    vk::raii::ShaderModule shader_module{device, sm_ci};

    // 2. 着色器阶段配置
    // 注意：Vert 和 Frag 在同一个 SPV 里（你的代码是这么写的）
    vk::PipelineShaderStageCreateInfo stages[2]{};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = *shader_module;
    stages[0].pName = "vertMain";

    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = *shader_module;
    stages[1].pName = "fragMain";

    // 3. 顶点输入 (Vertex Input) - terrain is not instanced here
    auto binding = VertexBindingDescriptions();
    auto attrs = VertexAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vi{};
    vi.sType = vk::StructureType::ePipelineVertexInputStateCreateInfo;
    vi.vertexBindingDescriptionCount = static_cast<uint32_t>(binding.size());
    vi.pVertexBindingDescriptions = binding.data();
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vi.pVertexAttributeDescriptions = attrs.data();

    // 4. 输入装配 (Input Assembly)
    vk::PipelineInputAssemblyStateCreateInfo ia{};
    ia.topology = vk::PrimitiveTopology::eTriangleList;

    // 5. 视口与裁剪 (使用动态状态，所以这里只需指定数量)
    vk::PipelineViewportStateCreateInfo vp_state{};
    vp_state.viewportCount = 1;
    vp_state.scissorCount = 1;

    // 6. 光栅化 (Rasterization)
    vk::PipelineRasterizationStateCreateInfo rs{};
    rs.polygonMode = vk::PolygonMode::eFill;
    rs.cullMode = vk::CullModeFlagBits::eNone;
    rs.frontFace = vk::FrontFace::eClockwise;
    rs.lineWidth = 1.0f;

    // 7. 深度测试 (Depth Stencil)
    vk::PipelineDepthStencilStateCreateInfo dss{};
    dss.depthTestEnable = 1;
    dss.depthWriteEnable = 1;
    dss.depthCompareOp = vk::CompareOp::eLess;

    // 8. 多重采样 (Multisample)
    vk::PipelineMultisampleStateCreateInfo ms{};
    ms.rasterizationSamples = vk::SampleCountFlagBits::e1;

    // 9. 颜色混合 (Color Blend)
    vk::PipelineColorBlendAttachmentState cb_att{};
    cb_att.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo cb{};
    cb.attachmentCount = 1;
    cb.pAttachments = &cb_att;

    // 10. 动态状态 (Dynamic State)
    std::array<vk::DynamicState, 2> dyn{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dyn_ci{};
    dyn_ci.dynamicStateCount = static_cast<uint32_t>(dyn.size());
    dyn_ci.pDynamicStates = dyn.data();

    // 12. 动态渲染信息 (Rendering Create Info - KHR_dynamic_rendering)
    vk::PipelineRenderingCreateInfo rendering_ci{};
    rendering_ci.colorAttachmentCount = 1;
    rendering_ci.pColorAttachmentFormats = &color_format;
    if (depth_format != vk::Format::eUndefined)
    {
      rendering_ci.depthAttachmentFormat = depth_format;
    }

    // 13. 创建图形管线
    vk::GraphicsPipelineCreateInfo gp_ci{};
    gp_ci.pNext = &rendering_ci; // 关键：指向动态渲染配置
    gp_ci.stageCount = 2;
    gp_ci.pStages = stages;
    gp_ci.pVertexInputState = &vi;
    gp_ci.pInputAssemblyState = &ia;
    gp_ci.pViewportState = &vp_state;
    gp_ci.pRasterizationState = &rs;
    gp_ci.pMultisampleState = &ms;
    gp_ci.pDepthStencilState = (depth_format != vk::Format::eUndefined) ? &dss : nullptr;
    gp_ci.pColorBlendState = &cb;
    gp_ci.pDynamicState = &dyn_ci;
    // gp_ci.layout = *pipeline_layout_;
    gp_ci.layout = *pass_resources_.pipeline_layout;
    gp_ci.renderPass = nullptr; // 使用动态渲染时设为 nullptr

    // 赋值给成员变量 pipeline_
    return vk::raii::Pipeline{device, nullptr, gp_ci};
  }

  vk::raii::Pipeline IRenderPass::CreateDepthPipeline(const vk::raii::Device &device,
                                                      const std::string &shader_path,
                                                      vk::Format color_format,
                                                      vk::Format depth_format)
  {
    // 加载 shadow.vert.spv (只有顶点着色器)
    auto const code = ReadFile(shader_path);
    vk::ShaderModuleCreateInfo sm_ci{.codeSize = code.size(), .pCode = (uint32_t *)code.data()};
    vk::raii::ShaderModule shader_module{device, sm_ci};

    vk::PipelineShaderStageCreateInfo stages[2]{};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = *shader_module;
    stages[0].pName = "vertMain";
    ;
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = *shader_module;
    stages[1].pName = "fragMain";

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
    gp_ci.layout = *pass_resources_.pipeline_layout; // 包含光源矩阵 UBO 的布局

    return vk::raii::Pipeline{device, nullptr, gp_ci};
  }

} // namespace vkfw
