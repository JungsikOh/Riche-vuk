#include "Editor.h"

void Editor::Initialize(GLFWwindow* window, VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VkUtils::QueueFamilyIndices queueFamily, VkQueue graphicsQueue)
{
	m_Window = window;

    m_Instance = instance;
    mainDevice.logicalDevice = device;
    mainDevice.physicalDevice = physicalDevice;

    m_QueueFamilyIndices = queueFamily;
    m_GraphicsQueue = graphicsQueue;

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
    init_info.QueueFamily = m_QueueFamilyIndices.graphicsFamily;
    init_info.Queue = m_GraphicsQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_ImguiDescriptorPool; // ImGui 전용 descriptor pool 생성 필요
    init_info.MinImageCount = MAX_FRAME_DRAWS;
    init_info.ImageCount = MAX_FRAME_DRAWS;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, renderPass);

    // 4. 초기화 이후 폰트 업로드
    VkCommandBuffer command_buffer = BeginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
    EndSingleTimeCommands(command_buffer);

    // Font texture 생성 후 cleanup
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void Editor::CreateImGuiDescriptorPool()
{
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    vkCreateDescriptorPool(mainDevice.logicalDevice, &pool_info, nullptr, &m_ImguiDescriptorPool);
}
