#pragma once

#include <vector>
#include <array>
#include <stdexcept>
#include <vulkan/vulkan.hpp>
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace vkfw
{
    inline vk::Format FindDepthFormat(vk::raii::PhysicalDevice const &pd)
    {
        std::array<vk::Format, 3> const candidates = {
            vk::Format::eD32Sfloat,
            vk::Format::eD24UnormS8Uint,
            vk::Format::eD32SfloatS8Uint,
        };

        for (auto fmt : candidates)
        {
            auto props = pd.getFormatProperties(fmt);
            if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
                return fmt;
        }
        throw std::runtime_error("No supported depth format");
    }

    inline uint32_t FindMemoryType(vk::raii::PhysicalDevice const &pd, uint32_t type_bits, vk::MemoryPropertyFlags required)
    {
        auto mem_props = pd.getMemoryProperties();
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i)
        {
            if ((type_bits & (1u << i)) == 0u)
                continue;
            if ((mem_props.memoryTypes[i].propertyFlags & required) == required)
                return i;
        }
        throw std::runtime_error("No suitable memory type");
    }

    inline void TransitionImage(vk::raii::CommandBuffer &cmd,
                                vk::Image image,
                                vk::ImageLayout old_layout,
                                vk::ImageLayout new_layout,
                                vk::ImageAspectFlags aspect,
                                vk::AccessFlags2 src_access,
                                vk::AccessFlags2 dst_access,
                                vk::PipelineStageFlags2 src_stage,
                                vk::PipelineStageFlags2 dst_stage)
    {
        vk::ImageMemoryBarrier2 barrier{};
        barrier.srcStageMask = src_stage;
        barrier.srcAccessMask = src_access;
        barrier.dstStageMask = dst_stage;
        barrier.dstAccessMask = dst_access;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspect;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vk::DependencyInfo dep{};
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &barrier;
        cmd.pipelineBarrier2(dep);
    }

    // -------------------------
    // Descriptor / buffer helpers
    // -------------------------

    inline vk::raii::DescriptorSetLayout CreateSingleBindingDescriptorSetLayout(
        const vk::raii::Device &device,
        uint32_t binding,
        vk::DescriptorType type,
        vk::ShaderStageFlags stage_flags,
        uint32_t descriptor_count = 1)
    {
        vk::DescriptorSetLayoutBinding b{};
        b.binding = binding;
        b.descriptorType = type;
        b.descriptorCount = descriptor_count;
        b.stageFlags = stage_flags;

        vk::DescriptorSetLayoutCreateInfo ci{};
        ci.bindingCount = 1;
        ci.pBindings = &b;
        return vk::raii::DescriptorSetLayout{device, ci};
    }

    inline vk::raii::DescriptorPool CreateSingleTypeDescriptorPool(
        const vk::raii::Device &device,
        vk::DescriptorType type,
        uint32_t descriptor_count,
        uint32_t max_sets)
    {
        vk::DescriptorPoolSize ps{type, descriptor_count};
        vk::DescriptorPoolCreateInfo ci{};
        ci.maxSets = max_sets;
        ci.poolSizeCount = 1;
        ci.pPoolSizes = &ps;
        return vk::raii::DescriptorPool{device, ci};
    }

    inline std::vector<vk::raii::DescriptorSet> AllocateDescriptorSets(
        const vk::raii::Device &device,
        const vk::raii::DescriptorPool &pool,
        const vk::raii::DescriptorSetLayout &layout,
        uint32_t count)
    {
        std::vector<vk::DescriptorSetLayout> layouts(count, *layout);
        vk::DescriptorSetAllocateInfo ai{};
        ai.descriptorPool = *pool;
        ai.descriptorSetCount = count;
        ai.pSetLayouts = layouts.data();
        return device.allocateDescriptorSets(ai);
    }

    inline void WriteUniformBufferDescriptor(
        const vk::raii::Device &device,
        const vk::DescriptorSet &set,
        uint32_t binding,
        vk::Buffer buffer,
        vk::DeviceSize range,
        vk::DeviceSize offset = 0)
    {
        vk::DescriptorBufferInfo bi{buffer, offset, range};
        vk::WriteDescriptorSet w{};
        w.dstSet = set;
        w.dstBinding = binding;
        w.descriptorCount = 1;
        w.descriptorType = vk::DescriptorType::eUniformBuffer;
        w.pBufferInfo = &bi;
        device.updateDescriptorSets({w}, {});
    }

    inline void WriteCombinedImageSamplerDescriptor(
        const vk::raii::Device &device,
        const vk::DescriptorSet &set,
        uint32_t binding,
        vk::Sampler sampler,
        vk::ImageView view,
        vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        vk::DescriptorImageInfo ii{sampler, view, layout};
        vk::WriteDescriptorSet w{};
        w.dstSet = set;
        w.dstBinding = binding;
        w.descriptorCount = 1;
        w.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        w.pImageInfo = &ii;
        device.updateDescriptorSets({w}, {});
    }

    inline void CreateMappedBuffers(
        const vk::raii::Device &device,
        const vk::raii::PhysicalDevice &pd,
        uint32_t count,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        std::vector<vk::raii::Buffer> &buffers,
        std::vector<vk::raii::DeviceMemory> &memories,
        std::vector<void *> &mapped_ptrs)
    {
        buffers.clear();
        memories.clear();
        mapped_ptrs.clear();

        buffers.reserve(count);
        memories.reserve(count);
        mapped_ptrs.resize(count, nullptr);

        for (uint32_t i = 0; i < count; ++i)
        {
            vk::BufferCreateInfo bci{};
            bci.size = size;
            bci.usage = usage;
            buffers.emplace_back(device, bci);

            auto req = buffers.back().getMemoryRequirements();
            vk::MemoryAllocateInfo mai{};
            mai.allocationSize = req.size;
            mai.memoryTypeIndex = FindMemoryType(pd, req.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            memories.emplace_back(device, mai);
            buffers.back().bindMemory(*memories.back(), 0);
            mapped_ptrs[i] = memories.back().mapMemory(0, size);
        }
    }

    inline void UnmapAndClearMappedBuffers(
        std::vector<vk::raii::DeviceMemory> &memories,
        std::vector<void *> &mapped_ptrs)
    {
        for (size_t i = 0; i < mapped_ptrs.size(); ++i)
        {
            if (i < memories.size() && mapped_ptrs[i])
            {
                memories[i].unmapMemory();
                mapped_ptrs[i] = nullptr;
            }
        }
        mapped_ptrs.clear();
        memories.clear();
    }
} // namespace Forest
