#include "vk_main.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkFrameSync.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/features/gbuffer/GBufferPass.hpp"
#include "vk/features/lighting/LightingPass.hpp"
#include "vk/features/post/PostProcessPass.hpp"
#include "vk/features/shadow/ShadowPass.hpp"
#include "vk/features/ui/ImGuiPass.hpp"
#include "vk/renderer/VkRenderer.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#define IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
#include <imgui_impl_vulkan.h>
#include <iostream>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

#include "vk/features/mesh/MeshPass.hpp"

namespace {
constexpr uint32_t kWidth = 800;
constexpr uint32_t kHeight = 600;
constexpr uint32_t kFramesInFlight = 2;

struct SimpleCamera {
  glm::vec3 pos{0.0f, 10.0f, 50.0f};
  float yaw = -90.0f;
  float pitch = -10.0f;
  float move_speed = 20.0f;
  float mouse_sens = 0.12f;

  glm::vec3 Front() const {
    float const yaw_r = glm::radians(yaw);
    float const pitch_r = glm::radians(pitch);
    glm::vec3 f{};
    f.x = std::cos(yaw_r) * std::cos(pitch_r);
    f.y = std::sin(pitch_r);
    f.z = std::sin(yaw_r) * std::cos(pitch_r);
    return glm::normalize(f);
  }

  glm::mat4 View() const {
    return glm::lookAt(pos, pos + Front(), glm::vec3{0.0f, 1.0f, 0.0f});
  }

  glm::mat4 Proj(float aspect) const {
    glm::mat4 p = glm::perspectiveRH_ZO(glm::radians(60.0f), aspect, 0.1f, 200.0f);
    p[1][1] *= -1.0f; // Vulkan 需要翻转 Y
    return p;
  }

  void AddMouseDelta(float dx, float dy) {
    yaw += dx * mouse_sens;
    pitch -= dy * mouse_sens;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);
  }
};


static void FramebufferResizeCallback(GLFWwindow* window, int, int)
{
  auto* resized = reinterpret_cast<bool*>(glfwGetWindowUserPointer(window));
  if (resized)
    *resized = true;
}
} // namespace

class VkMainApp::Impl {
public:
  int Run()
  {
    InitWindow();
    InitVulkanFramework();
    MainLoop();
    Cleanup();
    return 0;
  }

private:
  GLFWwindow* window_ = nullptr;
  bool framebuffer_resized_ = false;

  vkfw::VkContext ctx_{};
  vkfw::VkSwapchain swapchain_{};
  vkfw::VkFrameSync sync_{};
  vkfw::VkRenderer renderer_{};
  vkfw::ImGuiResources imu_resources_;
  bool _isStart = false;

  void InitWindow()
  {
    if (glfwInit() == GLFW_FALSE)
      throw std::runtime_error("glfwInit failed");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(kWidth, kHeight, "ForestRenderingVk", nullptr, nullptr);
    if (window_ == nullptr)
      throw std::runtime_error("glfwCreateWindow failed");

    glfwSetWindowUserPointer(window_, &framebuffer_resized_);
    glfwSetFramebufferSizeCallback(window_, FramebufferResizeCallback);
  }

  void InitVulkanFramework()
  {
    vkfw::ContextCreateInfo ci{};
    ci.window = window_;
    #ifdef NDEBUG
        ci.enable_validation = false;
    #else
        ci.enable_validation = true;
    #endif
        ctx_.Init(ci);

    sync_.Init(ctx_, kFramesInFlight);

    // Swapchain creates from the window size via VkContext::Window().
    vkfw::SwapchainInfo si{};
    swapchain_.Init(ctx_, si);
    sync_.EnsureRenderFinishedSize(ctx_, swapchain_.ImageCount());
  
    

    // Shadow -> GBuffer -> Post -> Lighting -> UI
    std::string modelPath = "res/47-mapletree/MapleTree.obj";
    renderer_.AddPass(std::make_unique<vkfw::MeshPass>(modelPath));
    // renderer_.AddPass(std::make_unique<vkfw::ShadowPass>());
    // renderer_.AddPass(std::make_unique<vkfw::GBufferPass>());
    // renderer_.AddPass(std::make_unique<vkfw::PostProcessPass>());
    // renderer_.AddPass(std::make_unique<vkfw::LightingPass>()); // draws the triangle for now
    renderer_.AddPass(std::make_unique<vkfw::ImGuiPass>());



    if (!renderer_.Create(ctx_, swapchain_, sync_))
      throw std::runtime_error("vkfw::VkRenderer::Create failed");
    InitImGuiDynamic();
  }

  void RecreateSwapchain()
  {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    while (w == 0 || h == 0) {
      glfwGetFramebufferSize(window_, &w, &h);
      glfwWaitEvents();
    }

    swapchain_.Recreate(ctx_);
    renderer_.OnSwapchainRecreated(ctx_, swapchain_, sync_);
    framebuffer_resized_ = false;
  }

  void MainLoop()
  {
    using clock = std::chrono::high_resolution_clock;
    auto start_time = clock::now();
    auto last_time = start_time;

    SimpleCamera camera{};
    bool mouse_look = false;
    double last_x = 0.0, last_y = 0.0;

    while (!glfwWindowShouldClose(window_)) {
      glfwPollEvents();
        // 2. 启动 ImGui 帧
      ImGui_ImplVulkan_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      // 3. 定义 UI 界面逻辑 (这就是你创建按钮的地方)
      {
          ImGui::Begin("debug interface"); // 创建一个窗口
          
          ImGui::Text("number of trees: 1024");
          
          // // 创建按钮：Button 返回 true 代表本帧被点击了
          // if (ImGui::Button("生成新随机森林")) {
          //     // 这里写点击按钮后要执行的逻辑
          //     std::cout << "正在重新种植森林..." << std::endl;
          // }

          static float treeScale = 1.0f;
          
          ImGui::SliderFloat("scale", &treeScale, 0.1f, 5.0f);
          ImGui::Checkbox("rotation", &_isStart);
          ImGui::End();
          ImGui::Render();
      }

      auto now = clock::now();
      float dt = std::chrono::duration<float>(now - last_time).count();
      float t  = std::chrono::duration<float>(now - start_time).count();
      last_time = now;
      if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window_, GLFW_TRUE);

      // RMB mouse look
      if (glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        if (!mouse_look) {
          mouse_look = true;
          glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
          glfwGetCursorPos(window_, &last_x, &last_y);
        }
        double x, y;
        glfwGetCursorPos(window_, &x, &y);
        camera.AddMouseDelta(static_cast<float>(x - last_x), static_cast<float>(y - last_y));
        last_x = x; last_y = y;
      } else if (mouse_look) {
        mouse_look = false;
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      }

      glm::vec3 f = camera.Front();
      glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3{0.0f, 1.0f, 0.0f}));
      float speed = camera.move_speed * dt;

      if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS) camera.pos += f * speed;
      if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS) camera.pos -= f * speed;
      if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS) camera.pos += r * speed;
      if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS) camera.pos -= r * speed;
      if (glfwGetKey(window_, GLFW_KEY_E) == GLFW_PRESS) camera.pos += glm::vec3{0,1,0} * speed;
      if (glfwGetKey(window_, GLFW_KEY_Q) == GLFW_PRESS) camera.pos -= glm::vec3{0,1,0} * speed;

      vkfw::FrameGlobals globals{};
      globals.view = camera.View();
      globals.proj = camera.Proj(float(swapchain_.Extent().width) / float(swapchain_.Extent().height));
      globals.camera_pos = camera.pos;
      globals.time_seconds = t;
      globals.delta_seconds = dt;

      if (!renderer_.DrawFrame(ctx_, swapchain_, sync_, _isStart, globals))
        RecreateSwapchain();
    }
  }

  void Cleanup()
  {
    ctx_.Device().waitIdle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (ctx_.IsInitialized()) {
      renderer_.Destroy(ctx_);
      swapchain_.Shutdown(ctx_);
      sync_.Shutdown(ctx_);
      ctx_.Shutdown();
    }

    if (window_ != nullptr) {
      glfwDestroyWindow(window_);
      window_ = nullptr;
    }

    glfwTerminate();
  }
const static VkFormat color_format = VK_FORMAT_B8G8R8A8_UNORM;
void InitImGuiDynamic() 
{
    // --- 1. 数据准备 ---
    std::cout<<"start"<<std::endl;
    auto window              = ctx_.Window();
    const auto& instance     = ctx_.Instance(); 
    const auto& device       = ctx_.Device();
    const auto& physicalDevice = ctx_.PhysicalDevice();
    const auto& graphicsQueue  = ctx_.GraphicsQueue();
    auto queueFamilyIndex    = ctx_.GraphicsQueueFamilyIndex();
    
    vk::Format swapChainFormat = swapchain_.SurfaceFormat().format; 
    vk::Format depthFormat     = vk::Format::eD32Sfloat;
    uint32_t imageCount        = swapchain_.ImageCount();

    // --- 2. 创建 RAII 描述符池 ---
    // ImGui 后端会分配多种类型的 descriptor，并且对 image sampler 数量有下限断言（见 IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE）。
    // 这里直接给一个足够大的通用池，避免初始化阶段 IM_ASSERT 直接 abort。
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

    // --- 3. ImGui 核心上下文设置 ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // --- 4. 初始化 GLFW 后端 ---
    ImGui_ImplGlfw_InitForVulkan(window, true);

    // --- 5. 初始化 Vulkan 后端 (动态渲染模式) ---
    
 ImGui_ImplVulkan_InitInfo initInfo = {};
 // 1. 强制清零整个结构体，防止任何隐藏字段（如 ApiVersion）是随机值
 memset(&initInfo, 0, sizeof(initInfo)); 

  // 0. 如果工程启用了 IMGUI_IMPL_VULKAN_NO_PROTOTYPES/VK_NO_PROTOTYPES，必须先加载函数指针，否则后端会 IM_ASSERT 直接终止。
  // external/CMakeLists.txt 里对 external_libs 设置了 IMGUI_IMPL_VULKAN_NO_PROTOTYPES（PUBLIC），所以这里必需。
  auto loader = [](const char* function_name, void* user_data) -> PFN_vkVoidFunction {
    VkInstance instance = reinterpret_cast<VkInstance>(user_data);
    if (auto fn = ::vkGetInstanceProcAddr(instance, function_name))
      return fn;
    return ::vkGetInstanceProcAddr(VK_NULL_HANDLE, function_name);
  };
  if (!ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, loader, (void*)(VkInstance)*ctx_.Instance())) {
    throw std::runtime_error("ImGui_ImplVulkan_LoadFunctions failed");
  }

  // 2. 基础信息
  initInfo.ApiVersion     = VK_API_VERSION_1_3;
  initInfo.Instance       = *ctx_.Instance();
  initInfo.PhysicalDevice = *ctx_.PhysicalDevice();
  initInfo.Device         = *ctx_.Device();
  initInfo.QueueFamily    = queueFamilyIndex;
  initInfo.Queue          = *graphicsQueue;
  initInfo.DescriptorPool = *imu_resources_.descriptorPool;
  initInfo.MinImageCount  = 2;
  initInfo.ImageCount     = imageCount;

  // 3. 动态渲染配置
  initInfo.UseDynamicRendering = true;
  initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  // 重点：手动设置 sType 
  initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  static VkFormat format = VK_FORMAT_B8G8R8A8_UNORM; // 确保格式与交换链一致
  initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &format;
  initInfo.CheckVkResultFn = [](VkResult err) {
    if (err != VK_SUCCESS)
      std::cerr << "[ImGui] Vulkan Error: " << static_cast<int>(err) << std::endl;
  };
  if (nullptr==imu_resources_.descriptorPool) {
    throw std::runtime_error("描述符池创建失败！");
  }
// 使用 static_cast 转换为底层句柄类型，再转为 void* 方便 cout 识别
std::cout << "[DEBUG] Instance: "       << (void*)(VkInstance)*ctx_.Instance() << std::endl;
std::cout << "[DEBUG] PhysicalDevice: " << (void*)(VkPhysicalDevice)*ctx_.PhysicalDevice() << std::endl;
std::cout << "[DEBUG] Device: "         << (void*)(VkDevice)*ctx_.Device() << std::endl;
std::cout << "[DEBUG] Size of InitInfo: " << (uint32_t)sizeof(ImGui_ImplVulkan_InitInfo) << std::endl;
// 结构体内部的已经是原始类型了，直接转 void*
std::cout << "[DEBUG] initInfo PD: "    << (void*)initInfo.PhysicalDevice << std::endl;
    ImGui_ImplVulkan_Init(&initInfo);
  std::cout<<"end"<<std::endl;
    // --- 6. 创建字体 ---
    // ImGui_ImplVulkan_CreateFontsTexture();
}
};

VkMainApp::VkMainApp() : impl_(new Impl()) {}
VkMainApp::~VkMainApp() = default;
VkMainApp::VkMainApp(VkMainApp&&) noexcept = default;
VkMainApp& VkMainApp::operator=(VkMainApp&&) noexcept = default;

int VkMainApp::Run() { return impl_->Run(); }

int RunVkTriangleApp()
{
  VkMainApp app;
  return app.Run();
}
