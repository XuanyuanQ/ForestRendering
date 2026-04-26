#include "vk/features/terrain/TerrainPass.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/renderer/FrameContext.hpp"
#include "vk/renderer/RenderTargets.hpp"
#include "vk/renderer/FrameGlobals.hpp"
#include "vk/scene/Vertex.hpp"
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace vkfw
{
  namespace
  {

    Model createQuad(float const width, float const height,
                     unsigned int const horizontal_split_count,
                     unsigned int const vertical_split_count)
    {
      unsigned int const xSegments = std::max(1u, horizontal_split_count);
      unsigned int const ySegments = std::max(1u, vertical_split_count);

      Model data{};
      data.vertices.reserve((xSegments + 1) * (ySegments + 1));
      data.indices.reserve(xSegments * ySegments * 6);

      glm::vec3 const n = {0.0f, 1.0f, 0.0f};
      for (unsigned int iy = 0; iy <= ySegments; ++iy)
      {
        float const v = static_cast<float>(iy) / static_cast<float>(ySegments);
        float const z = -0.5 * height + v * height;

        for (unsigned int ix = 0; ix <= xSegments; ++ix)
        {
          float const u = static_cast<float>(ix) / static_cast<float>(xSegments);
          float const x = -0.5f * width + u * width;

          data.vertices.push_back(Vertex{
              .pos = {x, 0.0f, z},
              .normal = n,
              .uv = {u, v},
          });
        }
      }

      uint32_t const stride = xSegments + 1;
      for (uint32_t iy = 0; iy < ySegments; ++iy)
      {
        for (uint32_t ix = 0; ix < xSegments; ++ix)
        {
          uint32_t const bl = ix + iy * stride;
          uint32_t const br = (ix + 1) + iy * stride;
          uint32_t const tl = ix + (iy + 1) * stride;
          uint32_t const tr = (ix + 1) + (iy + 1) * stride;

          data.indices.push_back(bl);
          data.indices.push_back(br);
          data.indices.push_back(tr);

          data.indices.push_back(tr);
          data.indices.push_back(tl);
          data.indices.push_back(bl);
        }
      }

      return data;
    }

  } // namespace

  void TerrainPass::CreateVertexBuffer(
      const VkContext &ctx,
      const vk::PhysicalDeviceMemoryProperties &mem_props,
      const Model &model)
  {
    vk::BufferCreateInfo bci{};

    bci.size = model.vertices.size() * sizeof(model.vertices[0]);
    bci.usage = vk::BufferUsageFlagBits::eVertexBuffer;
    bci.sharingMode = vk::SharingMode::eExclusive;
    vertex_buffer_ = vk::raii::Buffer{ctx.Device(), bci};

    auto req = vertex_buffer_.getMemoryRequirements();
    vk::MemoryAllocateInfo mai{};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemoryType(ctx.PhysicalDevice(), req.memoryTypeBits,
                                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    vertex_memory_ = vk::raii::DeviceMemory{ctx.Device(), mai};
    vertex_buffer_.bindMemory(*vertex_memory_, 0);

    void *dst = vertex_memory_.mapMemory(0, bci.size);
    std::memcpy(dst, model.vertices.data(), bci.size);
    vertex_memory_.unmapMemory();
  }

  void TerrainPass::CreateIndexBuffer(
      const VkContext &ctx,
      const vk::PhysicalDeviceMemoryProperties &mem_props,
      const Model &model)
  {
    vk::BufferCreateInfo bci{};

    bci.size = sizeof(model.indices[0]) * model.indices.size();
    bci.usage = vk::BufferUsageFlagBits::eIndexBuffer;
    bci.sharingMode = vk::SharingMode::eExclusive;
    index_buffer_ = vk::raii::Buffer{ctx.Device(), bci};

    auto req = index_buffer_.getMemoryRequirements();
    vk::MemoryAllocateInfo mai{};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemoryType(ctx.PhysicalDevice(), req.memoryTypeBits,
                                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    index_memory_ = vk::raii::DeviceMemory{ctx.Device(), mai};
    index_buffer_.bindMemory(*index_memory_, 0);

    void *dst = index_memory_.mapMemory(0, bci.size);
    std::memcpy(dst, model.indices.data(), bci.size);
    index_memory_.unmapMemory();
  }

  bool TerrainPass::Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets)
  {
    auto &device = ctx.Device();
    auto const mem_props = ctx.PhysicalDevice().getMemoryProperties();

    {
      terrtain_ = createQuad(500.0f, 500.0f, 1000, 1000);
      CreateVertexBuffer(ctx, mem_props, terrtain_);
      CreateIndexBuffer(ctx, mem_props, terrtain_);
    }

    LoadTexture(ctx, "res/forested-floor/textures/KiplingerFLOOR.png", 0);
    CreateCommonSampler(device);
    shader_path_ = "res/vk/terrain.spv";
    ShadowShader_path_ = "res/vk/shadow_terrarin.spv";
    
    if (!targets.shadow_map.Valid())
      throw std::runtime_error("TerrainPass requires RenderTargets::shadow_map");

    (void)swapchain;
    return true;
  }

  void TerrainPass::Destroy(VkContext &ctx)
  {
    ctx.Device().waitIdle();
    // TerrainPass 自身几何资源
    index_buffer_ = nullptr;
    index_memory_ = nullptr;
    vertex_buffer_ = nullptr;
    vertex_memory_ = nullptr;
    // 重构后 pass_resources_ 统一由基类清理
    ClearPassResources();
    IRenderPass::Destroy(ctx);
  }

  void TerrainPass::OnSwapchainRecreated(VkContext &, VkSwapchain const &, RenderTargets &)
  {
  }

  void TerrainPass::Record(FrameContext &frame, RenderTargets &targets)
  {

    if (!frame.cmd || !frame.globals)
      return;

    auto &cmd = *frame.cmd;
    uint32_t img = frame.image_index;

    CameraUBO ubo{};
    ubo.view = frame.globals->view;
    ubo.proj = frame.globals->proj;
    ubo.light = frame.globals->light;
    ubo.model = glm::mat4(1.0f);
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

    vk::ClearValue clear{};
    clear.color = vk::ClearColorValue(std::array<float, 4>{{0.0f, 0.0f, 0.0f, 1.0f}});

    vk::RenderingAttachmentInfo att{};
    att.imageView = frame.swapchain_image_view;
    att.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    att.loadOp = vk::AttachmentLoadOp::eLoad;
    att.storeOp = vk::AttachmentStoreOp::eStore;
    att.clearValue = clear;

    vk::RenderingInfo ri{};
    ri.renderArea.offset = vk::Offset2D{0, 0};
    ri.renderArea.extent = frame.swapchain_extent;
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &att;

    vk::RenderingAttachmentInfo depth_att{};
    if (targets.shared_depth.Valid())
    {
      depth_att.imageView = targets.shared_depth.view;
      depth_att.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
      depth_att.loadOp = vk::AttachmentLoadOp::eLoad;
      depth_att.storeOp = vk::AttachmentStoreOp::eStore;
      depth_att.clearValue = vk::ClearValue(vk::ClearDepthStencilValue{1.0f, 0});
      ri.pDepthAttachment = &depth_att;
    }

    cmd.beginRendering(ri);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pass_resources_.Colorpipeline);
    cmd.setViewport(0, vk::Viewport{0.0f, 0.0f, static_cast<float>(frame.swapchain_extent.width),
                                    static_cast<float>(frame.swapchain_extent.height), 0.0f, 1.0f});
    cmd.setScissor(0, vk::Rect2D{vk::Offset2D{0, 0}, frame.swapchain_extent});
    JustDraw(frame, cmd, *pass_resources_.pipeline_layout, img);
    cmd.endRendering();
  }

  void TerrainPass::JustDraw(FrameContext &frame, vk::raii::CommandBuffer &cmd, vk::PipelineLayout layout, uint32_t image_index)
  {
    cmd.bindIndexBuffer(*index_buffer_, 0, vk::IndexType::eUint32);
    cmd.bindVertexBuffers(0, *vertex_buffer_, {0});

      uint32_t const mat_count = static_cast<uint32_t>(textures_.size());
      uint32_t const idx = image_index * mat_count;
      if (image_index < frame.resources->ubo_ds_info.sets.size() && image_index < frame.resources->shadow_ds_info.sets.size() && idx < pass_resources_.material_ds_info.sets.size())
      {
        std::array<vk::DescriptorSet, 3> sets = {*frame.resources->ubo_ds_info.sets[image_index], *pass_resources_.material_ds_info.sets[idx], *frame.resources->shadow_ds_info.sets[image_index]};
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, sets, {});
      }
    

    cmd.drawIndexed(static_cast<uint32_t>(terrtain_.indices.size()), 1, 0, 0, 0);
  }

  void TerrainPass::RecordShadow(FrameContext &frame, vk::raii::CommandBuffer &cmd, vk::PipelineLayout layout, uint32_t image_index)
  {
    if (frame.globals && frame.resources && image_index < frame.resources->ubo_map.size() && frame.resources->ubo_map[image_index])
    {
      CameraUBO ubo{};
      ubo.view = frame.globals->view;
      ubo.proj = frame.globals->proj;
      ubo.light = frame.globals->light;
      ubo.model = glm::mat4(1.0f);
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
