#pragma once
class Mesh
{
    VkDevice m_Device;
    VkBuffer m_VertexBuffer = nullptr;
    VkDeviceMemory m_VertexBufferMemory;
    VkBuffer m_IndexBuffer = nullptr;
    VkDeviceMemory m_IndexBufferMemory;
    VkBuffer m_InstanceBuffer = nullptr;
    VkDeviceMemory m_InstanceBufferMemory;

public:
    // Only vb
    uint32_t vertexCount = 0;
    uint32_t startVertexLoc = 0; // Index of the first vertex

    // vb / ib
    uint32_t indexCount = 0;
    // the location of the first index read by the GPU from ib
    uint32_t startIndexLoc = 0;
    // A value added to each index before reading a vertex from the vb
    uint32_t baseVertexLoc = 0;

    // only instancing
    uint32_t instanceCount = 1;
    // A value added to each idx before reading per-instance data from a vb
    uint32_t startInstanceLoc = 0;

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

public:
    Mesh() = default;
    ~Mesh() = default;

    void Initialize(VkDevice device, uint32_t vertexSize, uint32_t indexSize);
    void Cleanup();

    VkBuffer& GetVkVertexBuffer();
    VkDeviceMemory& GetVkVertexDeviceMemory();
    VkBuffer& GetVkIndexBuffer();
    VkDeviceMemory& GetVkIndexDeviceMemory();

    uint32_t GetIndexCount();

};

