#pragma once

#include "Components.h"

class Mesh
{

    glm::mat4 m_Model;

public:
    std::vector<BasicVertex> vertices;
    std::vector<uint32_t> indices;

    // Only vb
    uint32_t vertexCount = 0;
    uint32_t startVertexLoc = 0; // Index of the first vertex, It' like offset

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

    int materialID = 0;

public:
    Mesh() = default;
    ~Mesh() = default;

    void Initialize(std::vector<BasicVertex> _vertices, std::vector<uint32_t> _indices);
    void Cleanup();

    uint32_t GetIndexCount();

    glm::mat4& GetModel();

};

