#pragma once

#define COMPONENTS

struct BasicVertex
{
    glm::vec3 pos;	// Vertex pos (x, y, z)
    glm::vec3 col;	// Vertex colour (r, g, b)
    glm::vec2 tex;	// Texture Coords (u, v)
};

struct COMPONENTS Mesh 
{
	VkBuffer _VertexBuffer = nullptr;
    VkBuffer _IndexBuffer = nullptr;
    VkBuffer _InstanceBuffer = nullptr;

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
};