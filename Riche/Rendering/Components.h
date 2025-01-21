#pragma once

#include "Utils/BoundingBox.h"

#define COMPONENTS

struct Model {
  glm::mat4 model;
};

struct ViewProjection {
  glm::mat4 view;
  glm::mat4 projection;
};

struct ShaderSetting {
#ifdef _DEBUG
  uint32_t isDebugging = true;
#else
  uint32_t isDebugging = false;
#endif  // _DEBUG
  uint32_t batchIdx = 0;
  float pad[2];
};

struct BasicVertex {
  glm::vec3 pos;     // Vertex pos (x, y, z)
  glm::vec3 normal;  // Vertex colour (r, g, b)
  glm::vec2 tex;     // Texture Coords (u, v)

  bool operator==(const BasicVertex& other) const { return pos == other.pos && normal == other.normal && tex == other.tex; }
};

struct COMPONENTS MaterialCPU {
  glm::vec4 baseColor = glm::vec4(1.0f);
  float albedoFactor = 1.0f;
  float metallicFactor = 1.0f;
  float roughnessFactor = 1.0f;
  float emissiveFactor = 1.0f;
};

struct MaterialBufferManager {
  std::vector<MaterialCPU> m_materialCPUList;

  VkBuffer m_materialGPUBuffer = VK_NULL_HANDLE;
  VkDeviceMemory m_materialGPUBufferMemory = VK_NULL_HANDLE;
  size_t m_materialCount = 0;

  uint32_t AddMaterial(const MaterialCPU& mat) {
    uint32_t idx = static_cast<uint32_t>(m_materialCPUList.size());
    m_materialCPUList.push_back(mat);
    return idx;
  }

  void UploadToGPU(VkDevice device) {

  }
};

struct COMPONENTS ObjectID {
  int materialID = 0;
  float dummy[3] = {0.0f};
};

struct COMPONENTS Transform {
  glm::mat4 startTransform = glm::mat4(1.0f);
  glm::mat4 currentTransform = glm::mat4(1.0f);
};

struct COMPONENTS BoundingBox {
  AABB originalBox;
  AABB currentBoxBox;
};