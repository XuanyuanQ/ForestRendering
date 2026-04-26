#include "vk/features/mesh/MeshPass.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/renderer/FrameContext.hpp"
#include "vk/renderer/FrameGlobals.hpp"
#include "vk/renderer/RenderTargets.hpp"
#include "vk/scene/Model.hpp"
#include "vk/scene/Vertex.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstring>
#include <stdexcept>
#include "vk/scene/stb_image.h"

#include <random>

namespace vkfw
{

  bool MeshPass::Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets)
  {
    auto &device = ctx.Device();
    sub_meshes_.clear();

    Model stemModel = Model::LoadFromFile("res/47-mapletree/MapleTreeStem.obj");
    Model leafModel = Model::LoadFromFile("res/47-mapletree/MapleTreeLeaves.obj");

    sub_meshes_.push_back({(uint32_t)stemModel.indices.size(), 0, 0});
    sub_meshes_.push_back({(uint32_t)leafModel.indices.size(), (uint32_t)stemModel.indices.size(), 1});

    std::vector<Vertex> all_vertices = stemModel.vertices;
    all_vertices.insert(all_vertices.end(), leafModel.vertices.begin(), leafModel.vertices.end());

    std::vector<uint32_t> all_indices = stemModel.indices;
    uint32_t stemVertexOffset = static_cast<uint32_t>(stemModel.vertices.size());
    for (auto idx : leafModel.indices)
    {
      all_indices.push_back(idx + stemVertexOffset);
    }
    total_index_count_ = static_cast<uint32_t>(all_indices.size());
    std::default_random_engine generator;
    std::uniform_real_distribution<float> pos_dist(-5.0f, 5.0f);
    std::uniform_real_distribution<float> rot_dist(0.0f, 360.0f);
    std::uniform_real_distribution<float> scale_dist(0.8f, 1.5f);

    std::vector<InstanceData> instance_matrices;
    int count = 10;
    float spacing = 30.0f;
    for (int x = -count; x < count; x++)
    {
      for (int z = -count; z < count; z++)
      {
        InstanceData data;

        float posX = x * spacing + pos_dist(generator);
        float posZ = z * spacing + pos_dist(generator);
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(posX, 0.0f, posZ));
        m = glm::rotate(m, glm::radians(rot_dist(generator)), glm::vec3(0, 1, 0));
        float s = scale_dist(generator);
        m = glm::scale(m, glm::vec3(s));

        data.model = m;
        instance_matrices.push_back(data);
      }
    }
    instance_count_ = static_cast<uint32_t>(instance_matrices.size());

    vk::DeviceSize buffer_size = sizeof(InstanceData) * instance_matrices.size();
    vk::BufferCreateInfo inst_ci{};
    inst_ci.size = buffer_size;
    inst_ci.usage = vk::BufferUsageFlagBits::eVertexBuffer;
    instance_buf_ = vk::raii::Buffer{device, inst_ci};

    auto inst_req = instance_buf_.getMemoryRequirements();
    vk::MemoryAllocateInfo inst_mai{};
    inst_mai.sType = vk::StructureType::eMemoryAllocateInfo;
    inst_mai.allocationSize = inst_req.size;
    inst_mai.memoryTypeIndex = FindMemoryType(ctx.PhysicalDevice(), inst_req.memoryTypeBits,
                                              vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    instance_mem_ = vk::raii::DeviceMemory{device, inst_mai};
    instance_buf_.bindMemory(*instance_mem_, 0);

    void *p_data = instance_mem_.mapMemory(0, buffer_size);
    std::memcpy(p_data, instance_matrices.data(), (size_t)buffer_size);
    instance_mem_.unmapMemory();

    {
      vk::DeviceSize vb_size = sizeof(Vertex) * all_vertices.size();
      vk::BufferCreateInfo vb_ci{};
      vb_ci.size = vb_size;
      vb_ci.usage = vk::BufferUsageFlagBits::eVertexBuffer;
      vb_ci.sharingMode = vk::SharingMode::eExclusive;
      vb_ = vk::raii::Buffer{device, vb_ci};

      auto req = vb_.getMemoryRequirements();
      vk::MemoryAllocateInfo vb_mai{};
      vb_mai.allocationSize = req.size;
      vb_mai.memoryTypeIndex = FindMemoryType(ctx.PhysicalDevice(), req.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
      vb_mem_ = vk::raii::DeviceMemory{device, vb_mai};
      vb_.bindMemory(*vb_mem_, 0);

      void *m = vb_mem_.mapMemory(0, vb_size);
      std::memcpy(m, all_vertices.data(), (size_t)vb_size);
      vb_mem_.unmapMemory();
    }

    {
      vk::DeviceSize ib_size = sizeof(uint32_t) * all_indices.size();
      vk::BufferCreateInfo ib_ci{};
      ib_ci.size = ib_size;
      ib_ci.usage = vk::BufferUsageFlagBits::eIndexBuffer;
      ib_ci.sharingMode = vk::SharingMode::eExclusive;
      ib_ = vk::raii::Buffer{device, ib_ci};

      auto ireq = ib_.getMemoryRequirements();
      vk::MemoryAllocateInfo ib_mai{};
      ib_mai.allocationSize = ireq.size;
      ib_mai.memoryTypeIndex = FindMemoryType(ctx.PhysicalDevice(), ireq.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
      ib_mem_ = vk::raii::DeviceMemory{device, ib_mai};
      ib_.bindMemory(*ib_mem_, 0);

      void *im = ib_mem_.mapMemory(0, ib_size);
      std::memcpy(im, all_indices.data(), (size_t)ib_size);
      ib_mem_.unmapMemory();
    }

    if (!targets.shared_depth.Valid())
      throw std::runtime_error("MeshPass requires RenderTargets::shared_depth");
    if (!targets.shadow_map.Valid())
      throw std::runtime_error("MeshPass requires RenderTargets::shadow_map");
    LoadTexture(ctx, "res/47-mapletree/maple_bark.png", 0);

    LoadTexture(ctx, "res/47-mapletree/maple_leaf.png", 1);

    CreateCommonSampler(device);
    shader_path_ = "res/vk/mesh.spv";
    ShadowShader_path_ = "res/vk/shadow_mesh.spv";
    (void)swapchain;

    return true;
  }

  void MeshPass::Destroy(VkContext &ctx)
  {
    ctx.Device().waitIdle();
    // MeshPass 自身 GPU 缓冲资源
    instance_buf_ = nullptr;
    instance_mem_ = nullptr;
    ib_ = nullptr;
    ib_mem_ = nullptr;
    vb_ = nullptr;
    vb_mem_ = nullptr;
    sub_meshes_.clear();
    // 重构后 pass_resources_ 统一由基类清理
    ClearPassResources();
    IRenderPass::Destroy(ctx);
  }

  void MeshPass::OnSwapchainRecreated(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &)
  {
    (void)ctx;
    (void)swapchain;
  }

  void MeshPass::Record(FrameContext &frame, RenderTargets &targets)
  {
    if (!frame.cmd || !frame.globals)
      return;
    if (!targets.shared_depth.Valid())
      return;

    auto &cmd = *frame.cmd;
    uint32_t img = frame.image_index;

    CameraUBO ubo{};
    ubo.view = frame.globals->view;
    ubo.proj = frame.globals->proj;
    ubo.light = frame.globals->light;
    ubo.model = model_matrix_;
    ubo.camera_pos = glm::vec4(frame.globals->camera_pos, 1.0f);
    ubo.shadow_params = glm::vec4((debugParameter_ && debugParameter_->shadowmap) ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);

    glm::vec3 const light_pos = frame.globals->light_position;
    float const len2 = glm::dot(light_pos, light_pos);
    glm::vec3 const dir_to_light = (len2 > 1e-6f) ? glm::normalize(light_pos) : glm::vec3(0.0f, 1.0f, 0.0f);
    ubo.light_dir = glm::vec4(dir_to_light, 0.0f);

    if (img < frame.resources->ubo_map.size() && frame.resources->ubo_map[img])
    {
      std::memcpy(frame.resources->ubo_map[img], &ubo, sizeof(ubo));
    }

    vk::ClearValue clear_color{};
    clear_color.color = vk::ClearColorValue(std::array<float, 4>{{0.05f, 0.06f, 0.08f, 1.0f}});

    vk::ClearValue clear_depth{};
    clear_depth.depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

    vk::RenderingAttachmentInfo color_att{};
    color_att.imageView = frame.swapchain_image_view;
    color_att.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_att.loadOp = vk::AttachmentLoadOp::eLoad;
    color_att.storeOp = vk::AttachmentStoreOp::eStore;
    color_att.clearValue = clear_color;

    vk::RenderingAttachmentInfo depth_att{};
    depth_att.imageView = targets.shared_depth.view;
    depth_att.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depth_att.loadOp = vk::AttachmentLoadOp::eLoad;
    depth_att.storeOp = vk::AttachmentStoreOp::eStore;
    depth_att.clearValue = clear_depth;

    vk::RenderingInfo ri{};
    ri.renderArea.offset = vk::Offset2D{0, 0};
    ri.renderArea.extent = frame.swapchain_extent;
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &color_att;
    ri.pDepthAttachment = &depth_att;

    cmd.beginRendering(ri);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pass_resources_.Colorpipeline);

    vk::Viewport vp{0.0f, 0.0f, (float)frame.swapchain_extent.width, (float)frame.swapchain_extent.height, 0.0f, 1.0f};
    cmd.setViewport(0, vp);

    vk::Rect2D sc{{0, 0}, frame.swapchain_extent};
    cmd.setScissor(0, sc);

    JustDraw(frame, cmd, *pass_resources_.pipeline_layout, img);
    cmd.endRendering();
  }

  void MeshPass::JustDraw(FrameContext &frame, vk::raii::CommandBuffer &cmd, vk::PipelineLayout layout, uint32_t image_index)
  {
    std::vector<vk::Buffer> vertex_buffers = {*vb_, *instance_buf_};
    std::vector<vk::DeviceSize> offsets = {0, 0};
    cmd.bindVertexBuffers(0, vertex_buffers, offsets);

    cmd.bindIndexBuffer(*ib_, 0, vk::IndexType::eUint32);

    for (const auto &sm : sub_meshes_)
    {
      uint32_t const idx = image_index * static_cast<uint32_t>(textures_.size()) + sm.textureIndex;

        if (image_index < frame.resources->ubo_ds_info.sets.size() &&
            image_index < frame.resources->shadow_ds_info.sets.size() &&
            idx < pass_resources_.material_ds_info.sets.size())
        {
          std::array<vk::DescriptorSet, 3> sets = {
              *frame.resources->ubo_ds_info.sets[image_index],
              *pass_resources_.material_ds_info.sets[idx],
              *frame.resources->shadow_ds_info.sets[image_index]};
          cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, sets, {});
        }
      

      cmd.drawIndexed(sm.indexCount, instance_count_, sm.firstIndex, 0, 0);
    }
  }

  void MeshPass::RecordShadow(FrameContext &frame, vk::raii::CommandBuffer &cmd, vk::PipelineLayout layout, uint32_t image_index)
  {
    if (frame.globals && frame.resources && image_index < frame.resources->ubo_map.size() && frame.resources->ubo_map[image_index])
    {
      CameraUBO ubo{};
      ubo.view = frame.globals->view;
      ubo.proj = frame.globals->proj;
      ubo.light = frame.globals->light;
      ubo.model = model_matrix_;
      ubo.camera_pos = glm::vec4(frame.globals->camera_pos, 1.0f);
      ubo.shadow_params = glm::vec4((debugParameter_ && debugParameter_->shadowmap) ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
      glm::vec3 const light_pos = frame.globals->light_position;
      float const len2 = glm::dot(light_pos, light_pos);
      glm::vec3 const dir_to_light = (len2 > 1e-6f) ? glm::normalize(light_pos) : glm::vec3(0.0f, 1.0f, 0.0f);
      ubo.light_dir = glm::vec4(dir_to_light, 0.0f);
      std::memcpy(frame.resources->ubo_map[image_index], &ubo, sizeof(ubo));
    }
    JustDraw(frame, cmd, layout, image_index);
  }

} // namespace vkfw
