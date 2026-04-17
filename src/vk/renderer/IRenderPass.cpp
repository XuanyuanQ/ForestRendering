#pragma once
#include "vk/renderer/helper.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "vk/scene/stb_image.h"
#include "vk/renderer/IRenderPass.hpp"

#include <iostream>
namespace vkfw
{

  TextureResource IRenderPass::LoadTextureResource(
      vk::raii::Device &device,
      vk::raii::PhysicalDevice &physDevice,
      vk::raii::Queue &queue,
      const std::string &path)
  {
    // 1. Load pixels
    int w, h, c;
    stbi_uc *pixels = stbi_load(path.c_str(), &w, &h, &c, STBI_rgb_alpha);
    if (!pixels)
    {
      throw std::runtime_error("Failed to load texture: " + path);
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

} // namespace vkfw
