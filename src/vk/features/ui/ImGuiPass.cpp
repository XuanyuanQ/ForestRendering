#include "vk/features/ui/ImGuiPass.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/renderer/FrameContext.hpp"
#include "vk/renderer/RenderTargets.hpp"
#include "vk/renderer/helper.hpp"

#include <imgui_impl_glfw.h>
#define IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING

#include <array>
#include <cstring>
#include <stdexcept>
#include <iostream>

namespace vkfw
{

  bool ImGuiPass::Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets)
  {
    InitImGuiDynamic(ctx, swapchain, targets);
    return true;
  }

  void ImGuiPass::InitImGuiDynamic(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets)
  {
    (void)targets;
    auto window = ctx.Window();
    const auto &device = ctx.Device();
    const auto &graphicsQueue = ctx.GraphicsQueue();
    auto queueFamilyIndex = ctx.GraphicsQueueFamilyIndex();

    vk::Format swapChainFormat = swapchain.SurfaceFormat().format;
    uint32_t imageCount = swapchain.ImageCount();

    std::array<vk::DescriptorPoolSize, 11> poolSizes = {
        vk::DescriptorPoolSize{vk::DescriptorType::eSampler, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformTexelBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageTexelBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformBufferDynamic, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageBufferDynamic, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eInputAttachment, 1000},
    };

    vk::DescriptorPoolCreateInfo poolInfo = vk::DescriptorPoolCreateInfo()
                                                .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
                                                .setMaxSets(1000 * static_cast<uint32_t>(poolSizes.size()))
                                                .setPoolSizeCount(static_cast<uint32_t>(poolSizes.size()))
                                                .setPPoolSizes(poolSizes.data());

    imu_resources_.descriptorPool = vk::raii::DescriptorPool(device, poolInfo);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo initInfo = {};
    memset(&initInfo, 0, sizeof(initInfo));
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = *ctx.Instance();
    initInfo.PhysicalDevice = *ctx.PhysicalDevice();
    initInfo.Device = *ctx.Device();
    initInfo.QueueFamily = queueFamilyIndex;
    initInfo.Queue = *graphicsQueue;
    initInfo.DescriptorPool = *imu_resources_.descriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = imageCount;

    initInfo.UseDynamicRendering = true;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    imgui_color_format_ = static_cast<VkFormat>(swapChainFormat);
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &imgui_color_format_;
    initInfo.CheckVkResultFn = [](VkResult err)
    {
      if (err != VK_SUCCESS)
        std::cerr << "[ImGui] Vulkan Error: " << static_cast<int>(err) << std::endl;
    };
    if (nullptr == imu_resources_.descriptorPool)
    {
      throw std::runtime_error("描述符池创建失败！");
    }
    ImGui_ImplVulkan_Init(&initInfo);
  }

  void ImGuiPass::Destroy(VkContext &ctx)
  {
    ctx.Device().waitIdle();
    debugParameter_ = nullptr;
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    imu_resources_.descriptorPool = nullptr;
    imgui_color_format_ = VK_FORMAT_UNDEFINED;
    IRenderPass::Destroy(ctx);
  }

  void ImGuiPass::OnSwapchainRecreated(VkContext &, VkSwapchain const &, RenderTargets &) {}

  void ImGuiPass::Record(FrameContext &frame, RenderTargets &targets)
  {
    (void)targets;

    if (frame.cmd == nullptr)
      return;

    auto &cmd = *frame.cmd;

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

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
      ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
      ImGui::Begin("debug interface");
      ImGui::Text("number of trees: 1024");

      static float treeScale = 1.0f;
      if (debugParameter_)
      {
        ImGui::Checkbox("Auto Sun (Animation)", &debugParameter_->animation);
        ImGui::Checkbox("Apply ShadowMap", &debugParameter_->shadowmap);
        ImGui::SliderFloat("Day Speed", &debugParameter_->daySpeed, 0.1f, 5.0f);
        ImGui::TextUnformatted("Uncheck Auto Sun to use manual Light X/Y/Z.");
        ImGui::SliderFloat("Light X", &debugParameter_->lightX, -200.0f, 200.0f);
        ImGui::SliderFloat("Light Y", &debugParameter_->lightY, -200.0f, 200.0f);
        ImGui::SliderFloat("Light Z", &debugParameter_->lightZ, -200.0f, 200.0f);
      }

      ImGui::SliderFloat("scale", &treeScale, 0.1f, 5.0f);
      ImGui::End();
      ImGui::Render();
    }

    cmd.beginRendering(ri);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cmd);

    cmd.endRendering();
    TransitionImage(cmd, frame.swapchain_image, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
                    vk::ImageAspectFlagBits::eColor, vk::AccessFlagBits2::eColorAttachmentWrite, {},
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe);
    frame.swapchain_old_layout = vk::ImageLayout::ePresentSrcKHR;
  }

  void ImGuiPass::setDebugParameter(DebugParam &param)
  {
    debugParameter_ = &param;
  }

} // namespace vkfw
