#pragma once

#include <cstdint>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace vkfw
{

  struct FrameGlobals;
  struct DescriptorSetInfos
  {
    vk::raii::DescriptorSetLayout layout{nullptr};
    vk::raii::DescriptorPool pool{nullptr};
    std::vector<vk::raii::DescriptorSet> sets{};

    // 必须支持移动构造，因为 RAII 对象不能拷贝
    DescriptorSetInfos() = default;
    DescriptorSetInfos(DescriptorSetInfos &&) noexcept = default;
    DescriptorSetInfos &operator=(DescriptorSetInfos &&) noexcept = default;

    // 禁用拷贝
    DescriptorSetInfos(const DescriptorSetInfos &) = delete;
    DescriptorSetInfos &operator=(const DescriptorSetInfos &) = delete;
  };

  struct FrameResource
  {
    // 全局管线（如果多个 Pass 共用）
    // vk::raii::PipelineLayout pipeline_layout{nullptr};
    // vk::raii::Pipeline Colorpipeline{nullptr};
    // vk::raii::Pipeline Depthpipeline{nullptr};

    // UBO 资源
    std::vector<vk::raii::Buffer> ubo_buf{};
    std::vector<vk::raii::DeviceMemory> ubo_mem{};
    std::vector<void *> ubo_map{};

    // 描述符信息包
    DescriptorSetInfos ubo_ds_info; // set=0
    // DescriptorSetInfos material_ds_info; // set=1
    DescriptorSetInfos shadow_ds_info; // set=2

    // 关键修复：
    FrameResource() = default;

    // 必须有 move 构造函数，且建议标记为 noexcept
    FrameResource(FrameResource &&other) noexcept = default;
    FrameResource &operator=(FrameResource &&other) noexcept = default;

    // 显式禁用拷贝（如果不写，编译器也会因为成员不可拷贝而报错，但写了更清晰）
    FrameResource(const FrameResource &) = delete;
    FrameResource &operator=(const FrameResource &) = delete;
  };
  struct PassResource
  {
    // 全局管线（如果多个 Pass 共用）
    vk::raii::PipelineLayout pipeline_layout{nullptr};
    vk::raii::Pipeline Colorpipeline{nullptr};
    vk::raii::Pipeline Depthpipeline{nullptr};

    // std::vector<vk::raii::Buffer> ubo_buf{};
    // std::vector<vk::raii::DeviceMemory> ubo_mem{};
    // std::vector<void *> ubo_map{};

    // 描述符信息包
    // DescriptorSetInfos ubo_ds_info;      // set=0
    DescriptorSetInfos material_ds_info; // set=1
    // DescriptorSetInfos shadow_ds_info;   // set=2

    PassResource() = default;

    // 必须有 move 构造函数，且建议标记为 noexcept
    PassResource(PassResource &&other) noexcept = default;
    PassResource &operator=(PassResource &&other) noexcept = default;

    // 显式禁用拷贝（如果不写，编译器也会因为成员不可拷贝而报错，但写了更清晰）
    PassResource(const PassResource &) = delete;
    PassResource &operator=(const PassResource &) = delete;
  };

  struct FrameContext
  {
    vk::raii::CommandBuffer *cmd = nullptr; // non-owning
    uint32_t frame_index = 0;
    uint32_t image_index = 0;
    vk::Extent2D swapchain_extent{};
    vk::Format swapchain_format{vk::Format::eUndefined};
    vk::Image swapchain_image{};
    vk::ImageView swapchain_image_view{};
    vk::ImageLayout swapchain_old_layout{vk::ImageLayout::ePresentSrcKHR};
    FrameGlobals const *globals = nullptr;    // non-owning
    FrameResource const *resources = nullptr; // non-owning
  };

} // namespace vkfw
