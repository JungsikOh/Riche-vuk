#include "Editor.h"

#include "Rendering/BasicLightingPass.h"
#include "Rendering/Camera.h"
#include "extern/tiny-stable-diffusion/TinyStableDiffusion.h"

static bool g_ShowFileBrowser = false;
static std::string g_SelectedFilePath = "";
static std::vector<std::string> g_AllowedExtensions = {".gltf"};

std::string ShowOpenFileDialog() {
  OPENFILENAMEA ofn;
  char szFile[260] = {0};

  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = nullptr;
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFilter = "All Files\0*.*\0PNG Files\0*.png\0JPEG Files\0*.jpg;*.jpeg\0\0";
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  if (GetOpenFileNameA(&ofn) == TRUE) {
    return std::string(szFile);
  }

  return std::string();
}

bool IsAllowedExtension(const std::filesystem::path& path) {
  // 폴더는 항상 표시
  if (std::filesystem::is_directory(path)) return true;

  std::string ext = path.extension().string();
  for (auto& allowed : g_AllowedExtensions) {
    if (ext == allowed) return true;
  }
  return false;
}

/*
 * File BrowserUI using ImGui
 */
void Editor::ShowFileBrowserUI(const std::string& filter) {
  static bool s_enableFilter = true;
  static std::string s_currentDir = std::filesystem::current_path().string();
  ImGui::Begin("File Browser", &g_ShowFileBrowser);

  ImGui::Text("Current Directory: %s", s_currentDir.c_str());

  if (ImGui::Button("Up")) {
    std::filesystem::path parent = std::filesystem::path(s_currentDir).parent_path();
    if (!parent.empty()) s_currentDir = parent.string();
  }
  ImGui::SameLine();
  ImGui::TextUnformatted("(Go up one directory)");

  ImGui::Checkbox("Enable Filter", &s_enableFilter);
  ImGui::SameLine();
  ImGui::Text("(Allowed: %s)", filter.c_str());

  ImGui::Separator();

  // Check directory
  try {
    for (const auto& entry : std::filesystem::directory_iterator(s_currentDir)) {
      bool isDir = entry.is_directory();
      std::string name = entry.path().filename().string();

      if (s_enableFilter && !isDir) {
        if (!IsAllowedExtension(entry.path())) continue;
      }

      if (isDir) {
        if (ImGui::Selectable((std::string("[DIR] ") + name).c_str(), false)) {
          s_currentDir = entry.path().string();
        }
      } else {
        // Click File → Execute 'loadGltfModel()' → Exit Browser
        if (ImGui::Selectable(name.c_str(), false)) {
          g_SelectedFilePath = entry.path().string();

          std::string directoryPath = entry.path().parent_path().string() + "/";
          directoryPath.erase(0, 2);  // remove "./"

          std::string fileName = entry.path().filename().string();

          std::vector<Mesh> meshes = {};
          loadGltfModel(mainDevice.logicalDevice, directoryPath, fileName, meshes, 1.0f);
          g_BatchManager.FlushMiniBatch(g_BatchManager.m_miniBatchList, g_ResourceManager);

          for (Mesh& mesh : meshes) {
            for (RayTracingVertex& vertex : mesh.ray_vertices) {
              g_BatchManager.m_allMeshVertices.push_back(vertex);
            }
            for (uint32_t& index : mesh.indices) {
              g_BatchManager.m_allMeshIndices.push_back(index);
            }
            mesh.vertexOffset = g_BatchManager.m_accmulatedVertexOffset;
            mesh.indexOffset = g_BatchManager.m_accmulatedIndexOffset;
            g_BatchManager.m_accmulatedVertexOffset += mesh.ray_vertices.size() * sizeof(RayTracingVertex);
            g_BatchManager.m_accmulatedIndexOffset += mesh.indices.size() * sizeof(uint32_t);
            g_BatchManager.m_meshes.push_back(mesh);
          }

          g_BatchManager.RebuildBatchManager(mainDevice.logicalDevice, mainDevice.physicalDevice);
          m_pCullingPass->SetupQueryPool();
          m_pLightingPass->RebuildAS();

          g_ShowFileBrowser = false;

          break;
        }
      }
    }
  } catch (const std::exception& e) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", e.what());
  }

  ImGui::End();
}

void Editor::ShowStableDiffusionUI() {
  static char s_promptBuf[256] = "A peaceful lakeside cabin";
  static bool s_isGenerating = false;
  static std::future<void> s_genFuture;
  static std::string s_statusMessage = "";

  ImGui::Begin("Stable Diffusion", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

  ImGui::Text("Enter Prompt:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(300.0f);
  ImGui::InputText("##PromptInput", s_promptBuf, IM_ARRAYSIZE(s_promptBuf));

  ImGui::Spacing();

  if (ImGui::Button("Generate", ImVec2(120, 0))) {
    if (!s_isGenerating) {
      s_isGenerating = true;
      s_statusMessage = "Generating...";

      std::string prompt = s_promptBuf;
      s_genFuture = g_ThreadPool.Submit([prompt]() { GenerateImage2D(prompt); });
    }
  }
  ImGui::SameLine();
  ImGui::TextUnformatted(s_statusMessage.c_str());

  if (s_isGenerating) {
    auto status = s_genFuture.wait_for(std::chrono::milliseconds(0));
    if (status == std::future_status::ready) {
      s_genFuture.get();
      s_statusMessage = "Generation Completed!";
      s_isGenerating = false;
    }
  }

  ImGui::End();
}

void Editor::DrawTextureListUI() {
  ImGui::Begin("Texture Manager");

  static int selectedIndex = -1;
  static bool showFilePicker = false;

  if (ImGui::BeginTable("TextureTable", 5)) {
    for (int i = 0; i < (int)g_BatchManager.m_diffuseImages.size(); i++) {
      ImGui::PushID(i);
      ImGui::TableNextColumn();

      ImGui::Text(std::string("Texture #" + std::to_string(i)).c_str());
      if (ImGui::ImageButton("", (ImTextureID)g_BatchManager.m_textureIdList[i], ImVec2(64, 64))) {
        selectedIndex = i;

        std::string selectedFile = ShowOpenFileDialog();
        if (!selectedFile.empty()) {
          g_BatchManager.ChangeTexture(mainDevice.logicalDevice, mainDevice.physicalDevice, i, selectedFile);
          std::cout << selectedFile << std::endl;
        }

        showFilePicker = true;
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  ImGui::End();
}

void Editor::Initialize(GLFWwindow* window, VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice,
                        VkUtils::QueueFamilyIndices queueFamily, VkQueue graphicsQueue, Camera* camera) {
  m_Window = window;
  m_pCamera = camera;

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

void Editor::Update() {
  OnLeftMouseClick();
  UpdateKeyboard();
}

void Editor::RenderImGui(VkCommandBuffer commandBuffer, uint32_t currentImage) {
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

  /*
   * Main menu bar
   */
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Load Model")) {
        g_ShowFileBrowser = true;  // 파일 브라우저 열기
      }
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

  if (g_ShowFileBrowser) {
    ShowFileBrowserUI(".gltf");
  }

  ShowStableDiffusionUI();
  DrawTextureListUI();

  ImGui::Begin("Performance");
  ImGui::Text("Current FPS: %.1f", fps);
  ImGui::Text("Max FPS: %.1f | Average FPS: %.1f", maxFps, averageFps);
  ImGui::Text("Number Of Rendering Object (Before Culling) : %d", g_RenderSetting.beforeCullingRenderingNum);
  ImGui::Text("Number Of Rendering Object (After View Culling) : %d", g_RenderSetting.afterViewCullingRenderingNum);
  ImGui::End();

  ImGui::Begin("Rendering");
  ImGui::Checkbox("Multi Threading Culling", &(g_RenderSetting.isMultiThreading));
  ImGui::Checkbox("Wire Frame", &(g_RenderSetting.isWireRendering));
  ImGui::Checkbox("Occlusion Culling", &(g_RenderSetting.isOcclusionCulling));
  ImGui::Checkbox("View BoundingBox", &(g_RenderSetting.isRenderBoundingBox));
  ImGui::SliderFloat4("Light Pos", glm::value_ptr(g_ShaderSetting.lightPos), -5.0f, 5.0f);
  ImGui::Text("Selected File: %s", g_SelectedFilePath.c_str());
  ImGui::End();

  ImGuizmo::BeginFrame();

  if (m_selectedIndex >= 0 && m_selectedIndex < (int)g_BatchManager.m_transforms[currentImage].size() && m_gizmoType != -1) {
    g_RenderSetting.changeFlag = true;
    ImGuizmo::SetOrthographic(false);  // Persp / Ortho 설정
    ImGuizmo::Enable(true);

    float width = ImGui::GetIO().DisplaySize.x;
    float height = ImGui::GetIO().DisplaySize.y;
    ImGuizmo::SetRect(0, 0, width, height);

    glm::mat4 view = m_pCamera->View();
    glm::mat4 proj = m_pCamera->Proj();
    proj[1][1] *= -1.0f;

    glm::vec3 aabbCenter =
        (g_BatchManager.m_boundingBoxList[m_selectedIndex].min + g_BatchManager.m_boundingBoxList[m_selectedIndex].max) * 0.5f;
    {
      glm::mat4& tc = g_BatchManager.m_transforms[currentImage][m_selectedIndex].currentTransform;
      glm::mat4 transform = glm::translate(tc, aabbCenter);
      ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                           (ImGuizmo::OPERATION)m_gizmoType,  // 또는 필요한 조작 종류 (ROTATE, SCALE 등)
                           ImGuizmo::MODE::LOCAL,             // 월드 좌표계 또는 로컬 좌표계 선택
                           glm::value_ptr(transform));        // gizmo 행렬

      if (ImGuizmo::IsUsing()) {
        tc = transform * glm::translate(glm::mat4(1.0f), -aabbCenter);
      }

      for (int i = 0; i < MAX_FRAME_DRAWS; ++i) {
        g_BatchManager.m_transforms[i][m_selectedIndex].currentTransform = tc;
      }
    }
  }

  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void Editor::OnLeftMouseClick() {
  if (m_pCamera->isMousePressed) {
    m_selectedIndex = (int)m_pCamera->result.r;
  }
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

void Editor::UpdateKeyboard() {
  ImGuiIO& io = ImGui::GetIO();

  if (ImGui::IsKeyDown(ImGuiKey_Z)) {
    m_gizmoType = ImGuizmo::OPERATION::TRANSLATE;
  }
  if (ImGui::IsKeyDown(ImGuiKey_X)) {
    m_gizmoType = ImGuizmo::OPERATION::SCALE;
  }
  if (ImGui::IsKeyDown(ImGuiKey_C)) {
    m_gizmoType = ImGuizmo::OPERATION::ROTATE;
  }
}
