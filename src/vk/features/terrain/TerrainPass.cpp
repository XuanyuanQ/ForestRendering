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

  void TerrainPass::CreateInstanceBuffer(
    const VkContext &ctx,
    const std::vector<TerrainInstanceData>& instance_data)
{

    vk::BufferCreateInfo bci{};
    bci.size = instance_data.size() * sizeof(TerrainInstanceData);
    // 🔴 注意：虽然它是实例数据，但在 Vulkan 眼里它依然充当顶点输入的角色，所以用 eVertexBuffer
    bci.usage = vk::BufferUsageFlagBits::eVertexBuffer; 
    bci.sharingMode = vk::SharingMode::eExclusive;
    
    instance_buffer_ = vk::raii::Buffer{ctx.Device(), bci};

    auto req = instance_buffer_.getMemoryRequirements();
    vk::MemoryAllocateInfo mai{};
    mai.allocationSize = req.size;
    // 🟢 采用你熟悉的 HostVisible + HostCoherent，CPU 直接写，不需要任何 Staging 命令行搬运！
    mai.memoryTypeIndex = FindMemoryType(ctx.PhysicalDevice(), req.memoryTypeBits,
                                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    
    instance_memory_ = vk::raii::DeviceMemory{ctx.Device(), mai};
    instance_buffer_.bindMemory(*instance_memory_, 0);

    // CPU 直接灌入 900 个小块的世界坐标偏移量
    void *dst = instance_memory_.mapMemory(0, bci.size);
    std::memcpy(dst, instance_data.data(), bci.size);
    instance_memory_.unmapMemory();
}

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
    index_count_ = static_cast<uint32_t>(model.indices.size());
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
      // terrtain_ = createQuad(500.0f, 500.0f, 1000, 1000);
      Model terrtain_ = createQuad(16.0f, 16.0f, 32, 32);
      CreateVertexBuffer(ctx, mem_props, terrtain_);
      CreateIndexBuffer(ctx, mem_props, terrtain_);
      
      // =================================================================
      // 2. 生成拼接矩阵/偏移量 (Instance Data)
      // =================================================================
      int const grid_size_x = 30; // X方向拼接 30 个块
      int const grid_size_z = 30; // Z方向拼接 30 个块
      instance_count_ = grid_size_x * grid_size_z; // 总共 900 个实例

      std::vector<TerrainInstanceData> instance_data;
      float const chunk_width = 16.0f; // 必须和小块的实际尺寸严格一致，才能完美拼接

        for (int x = 0; x < grid_size_x; ++x) {
            for (int z = 0; z < grid_size_z; ++z) {
                TerrainInstanceData data{};
                
                // 计算平移位置
                float posX = (static_cast<float>(x) - grid_size_x / 2.0f) * chunk_width;
                float posZ = (static_cast<float>(z) - grid_size_z / 2.0f) * chunk_width;
                
                // 🟢 直接生成平移矩阵
                data.model = glm::translate(glm::mat4(1.0f), glm::vec3(posX, 0.0f, posZ));
                
                instance_matrices_.push_back(data);
            }
        }
      instance_count_ = static_cast<uint32_t>(instance_matrices_.size());
      CreateInstanceBuffer(ctx, instance_matrices_);
    }

    LoadTexture(ctx, "res/forested-floor/textures/KiplingerFLOOR.png", 0);
    CreateCommonSampler(device);
    shader_path_ = "res/vk/terrain.spv";
    ShadowShader_path_ = "res/vk/shadow_terrarin.spv";
    GBufferShader_path_ = "res/vk/terrain_gbuffer.spv";
    
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
    ubo.view_inv = glm::inverse(ubo.view);
    ubo.proj_inv = glm::inverse(ubo.proj);
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

    // glm::mat4 view_inv = glm::inverse(frame.globals->view);
    // glm::vec3 camera_pos = frame.globals->camera_pos;
    // glm::vec3 camera_forward = -glm::normalize(glm::vec3(view_inv[2]));
    // std::vector<TerrainInstanceData> visible_instances;
    // // 假设 instance_matrices_initial_ 是你在 Create 里生成的 400 个原始树位置
    // for (const auto& instance : instance_matrices_)
    // {
    //   glm::vec3 tree_pos = glm::vec3(instance.model[3]); // 树的世界坐标
        
    //     // 1. 算出生长向量：从相机指向树
    //     glm::vec3 cam_to_tree = tree_pos - camera_pos;
    //     float dist = glm::length(cam_to_tree);
        
    //     // 防止和相机重合导致除以 0
    //     if (dist < 0.1f) {
    //         visible_instances.push_back(instance);
    //         continue;
    //     }
        
    //     // 2. 归一化方向
    //     glm::vec3 dir_to_tree = cam_to_tree / dist;
        
    //     // 3. 【核心数学点积】：计算相机朝向和树木方向的夹角余弦值
    //     float dot_product = glm::dot(camera_forward, dir_to_tree);
        
    //     // 4. 判定条件：
    //     // dot_product > 0.5f 代表不仅在前面，而且大概在相机前方 120 度的视野（FOV）内
    //     // 如果你只想严格剔除正后方，改成 dot_product > 0.0f 即可
    //     // 同时保留一个远景裁剪（比如超过 150 米太远看不见的也扔掉）
    //     if (dot_product > 0.0f) 
    //     {
    //         visible_instances.push_back(instance);
    //     }
    // }
    // // 更新这一帧真正要画的实例数量
    // instance_count_ = static_cast<uint32_t>(visible_instances.size());
    // // printf("Visible trees: %u\n", instance_count_);
    // // 如果这一帧一棵树都看不见，直接返回
    // debugParameter_->treeCount = instance_count_;
    // if (instance_count_ == 0) return;
    // // 把挑出来的可见树矩阵，刷进 GPU 能够得到的内存里
    // // 注：因为你的内存是 HostCoherent 的，直接 map/memcpy 即可
    // void* p_data = instance_memory_.mapMemory(0, sizeof(InstanceData) * visible_instances.size());
    // std::memcpy(p_data, visible_instances.data(), sizeof(InstanceData) * visible_instances.size());
    // instance_memory_.unmapMemory();

    std::vector<vk::Buffer> vertex_buffers = {*vertex_buffer_, *instance_buffer_};
    std::vector<vk::DeviceSize> offsets = {0, 0};
    cmd.bindVertexBuffers(0, vertex_buffers, offsets);
    cmd.bindIndexBuffer(*index_buffer_, 0, vk::IndexType::eUint32);

      uint32_t const mat_count = static_cast<uint32_t>(textures_.size());
      uint32_t const idx = image_index * mat_count;
      if (image_index < frame.resources->ubo_ds_info.sets.size() && image_index < frame.resources->shadow_ds_info.sets.size() && idx < pass_resources_.material_ds_info.sets.size())
      {
        std::array<vk::DescriptorSet, 3> sets = {*frame.resources->ubo_ds_info.sets[image_index], *pass_resources_.material_ds_info.sets[idx], *frame.resources->shadow_ds_info.sets[image_index]};
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, sets, {});
      }
    

    // cmd.drawIndexed(static_cast<uint32_t>(terrtain_.indices.size()), 1, 0, 0, 0);
    cmd.drawIndexed(static_cast<uint32_t>(index_count_), instance_count_, 0, 0, 0);
  }


} // namespace vkfw
