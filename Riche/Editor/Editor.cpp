#include "Editor.h"

#include "VkUtils/ChooseFunc.h"

void Editor::Initialize(GLFWwindow* window, VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice,
                        VkUtils::QueueFamilyIndices queueFamily, VkQueue graphicsQueue) {
  m_Window = window;

  m_Instance = instance;
  mainDevice.logicalDevice = device;
  mainDevice.physicalDevice = physicalDevice;

  m_queueFamilyIndices = queueFamily;
  m_graphicsQueue = graphicsQueue;

  CreateImGuiDescriptorPool();
  // Array of Subpasses
  std::array<VkSubpassDescription, 1> subpasses{};

  //
  // ATTACHMENTS
  //
  // SwapChain Colour attacment of render pass
  VkAttachmentDescription swapChainColourAttachment = {};
  swapChainColourAttachment.format =
      VkUtils::ChooseSupportedFormat(mainDevice.physicalDevice, {VK_FORMAT_R8G8B8A8_UNORM}, VK_IMAGE_TILING_OPTIMAL,
                                     VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);  // format to use for attachment
  swapChainColourAttachment.samples =
      VK_SAMPLE_COUNT_1_BIT;  // number of samples to write for multisampling, relative to multisampling
  swapChainColourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;             // Describes what to do with attachment before rendering
  swapChainColourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;           // Describes what to do with attachment after rendering
  swapChainColourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // Describes what to do with stencil before rendering
  swapChainColourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // Describes what to do with stencil after rendering

  // FrameBuffer data will be stored as an image, but images can be given different data layouts
  // to give optimal use for certain operations, initial -> subpass -> final
  swapChainColourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;      // Image data layout before render pass starts
  swapChainColourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // Image data layout after render pass (to change to)

  VkAttachmentDescription depthStencilAttachment = {};
  depthStencilAttachment.format = VkUtils::ChooseSupportedFormat(
      mainDevice.physicalDevice, {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT},
      VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
  depthStencilAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthStencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthStencilAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthStencilAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthStencilAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // REFERENCES
  // FrameBuffer를 만들 때 사용하였던, attachment 배열을 참조한다고 보면 된다.
  // Attachment reference uses an attachment index that refers to index in the attachment list passed to renderPassCreateInfo
  VkAttachmentReference swapChainColourAttachmentRef = {};
  swapChainColourAttachmentRef.attachment = 0;  // 얼마나 많은 attachment가 있는지 정의X, 몇번째 attachment를 참조하냐의 느낌.
  swapChainColourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthStencilAttachmentRef = {};
  depthStencilAttachmentRef.attachment = 1;
  depthStencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // NOTE: We don't need Input Reference, because we don't use more than two subpasses.
  // Information about a particular subpass the render pass is using
  subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;  // Pipeline type subpass is to be bound to
  subpasses[0].colorAttachmentCount = 1;
  subpasses[0].pColorAttachments = &swapChainColourAttachmentRef;
  subpasses[0].pDepthStencilAttachment = &depthStencilAttachmentRef;

  //
  // SUBPASS DEPENDENCY
  //
  // Need to determine when layout transitions occur using subpass dependencies
  std::array<VkSubpassDependency, 2> subpassDependencies;

  // Conversion from VK_IMAGE_LAYER-UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
  // Transition must happen after ..
  subpassDependencies[0].srcSubpass =
      VK_SUBPASS_EXTERNAL;  // 외부에서 들어오므로, Subpass index(VK_SUBPASS_EXTERNAL = Special value meaning outside of renderpass)
  subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;  // Pipeline stage
  subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;            // Stage access mask (memory access)
  // But most happen before ..
  subpassDependencies[0].dstSubpass = 0;
  subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  subpassDependencies[0].dependencyFlags = 0;

  // Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
  // Transition must happen after ..
  subpassDependencies[1].srcSubpass = 0;
  subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;  // Pipeline stage
  subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  // But most happen before ..
  subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  subpassDependencies[1].dependencyFlags = 0;

  //
  // RENDER PASS CREATE INFO
  //
  std::array<VkAttachmentDescription, 2> renderPassAttachments = {swapChainColourAttachment, depthStencilAttachment};

  // Create info for Render Pass
  VkRenderPassCreateInfo renderPassCreateInfo = {};
  renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
  renderPassCreateInfo.pAttachments = renderPassAttachments.data();
  renderPassCreateInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
  renderPassCreateInfo.pSubpasses = subpasses.data();
  renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
  renderPassCreateInfo.pDependencies = subpassDependencies.data();

  VK_CHECK(vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass));

  // 1. ImGui Context 생성
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();

  // 2. ImGui 스타일 설정
  ImGui::StyleColorsDark();

  // 3. ImGui Vulkan 초기화 정보 설정
  ImGui_ImplGlfw_InitForVulkan(window, true);
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = m_Instance;
  init_info.PhysicalDevice = mainDevice.physicalDevice;
  init_info.Device = mainDevice.logicalDevice;
  init_info.QueueFamily = m_queueFamilyIndices.graphicsFamily;
  init_info.Queue = m_graphicsQueue;
  init_info.PipelineCache = VK_NULL_HANDLE;
  init_info.DescriptorPool = m_ImguiDescriptorPool;  // ImGui 전용 descriptor pool 생성 필요
  init_info.MinImageCount = MAX_FRAME_DRAWS;
  init_info.ImageCount = MAX_FRAME_DRAWS;
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.RenderPass = renderPass;

  ImGui_ImplVulkan_Init(&init_info);
  ImGui_ImplVulkan_CreateFontsTexture();
  // Font texture 생성 후 cleanup
  ImGui_ImplVulkan_DestroyFontsTexture();
}

void Editor::Cleanup() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  vkDestroyDescriptorPool(mainDevice.logicalDevice, m_ImguiDescriptorPool, nullptr);
  vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);
}

void Editor::RenderImGui(VkCommandBuffer commandBuffer) {
  static float fps;
  static std::chrono::high_resolution_clock::time_point lastTime;
  static float maxFps = 0.0f;
  static float minFps = 999999.0f;
  static float totalFps = 0.0f;
  static int frameCount = 0;

  auto currentTime = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float> elapsedTime = currentTime - lastTime;
  lastTime = currentTime;
  float deltaTime = elapsedTime.count();

  // FPS 계산
  if (deltaTime > 0.0f) {
    fps = 1.0f / deltaTime;
    frameCount++;
    totalFps += fps;

    // 최고 FPS 갱신
    if (fps > maxFps) {
      maxFps = fps;
    }
  }

  float averageFps = (frameCount > 0) ? totalFps / frameCount : 0.0f;

  // 새로운 ImGui 프레임 시작
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  // FPS 정보를 표시하는 창
  ImGui::Begin("Performance");
  ImGui::Text("Current FPS: %.1f: (frame rate : %.5f)", fps, 1 / fps * 1000.0f);
  ImGui::Text("Max FPS: %.1f | Average FPS: %.1f", maxFps, averageFps);
  ImGui::End();

  ImGui::Begin("Rendering");
  ImGui::Checkbox("active", &(g_RenderSetting.isWireRendering));
  ImGui::End();

  // ImGui 렌더링
  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void Editor::CreateImGuiDescriptorPool() {
  VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                       {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                       {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                       {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                       {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                       {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                       {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                       {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                       {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                       {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                       {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};
  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
  pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;
  vkCreateDescriptorPool(mainDevice.logicalDevice, &pool_info, nullptr, &m_ImguiDescriptorPool);
}