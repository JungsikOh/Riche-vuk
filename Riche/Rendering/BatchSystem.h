#pragma once

#include "Buffer.h"
#include "Components.h"
#include "Image.h"
#include "Mesh.h"
#include "Utils/Boundingbox.h"
#include "Utils/Singleton.h"
#include "VkUtils/DescriptorBuilder.h"
#include "VkUtils/DescriptorManager.h"
#include "VkUtils/ResourceManager.h"

static const uint32_t MAX_BATCH_SIZE = 3 * 1024 * 1024;  // 3MB
static std::vector<BasicVertex> s_accumulatedVertices;
static std::vector<uint32_t> s_accumulatedIndices;
static size_t s_accumulatedVertexSize;
static size_t s_accumulatedIndexSize;
static uint64_t s_accumulatedIndirectOffset;
static uint32_t s_accumulatedMeshIndex;

struct MiniBatch {
  VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
  VkBuffer m_indexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;

  uint32_t m_currentVertexOffset = 0;
  uint32_t m_currentIndexOffset = 0;
  uint32_t m_currentBatchSize = 0;

  uint64_t m_indirectCommandsOffset = 0;
  std::vector<VkDrawIndexedIndirectCommand> m_drawIndexedCommands;

  void Cleanup(VkDevice device) {
    vkDestroyBuffer(device, m_vertexBuffer, nullptr);
    vkFreeMemory(device, m_vertexBufferMemory, nullptr);

    vkDestroyBuffer(device, m_indexBuffer, nullptr);
    vkFreeMemory(device, m_indexBufferMemory, nullptr);
  }
};

class BatchManager : public Singleton<BatchManager> {
  friend class Singleton<BatchManager>;

 public:
  BatchManager() = default;
  ~BatchManager() = default;

  void Update(VkDevice device, uint32_t imageIndex);
  void UpdateDescriptorSets(VkDevice device);

  void Cleanup(VkDevice device);

  void AddDataToMiniBatch(std::vector<MiniBatch>& miniBatches, VkUtils::ResourceManager& manager, const Mesh& mesh, bool flag = false);
  void FlushMiniBatch(std::vector<MiniBatch>& miniBatches, VkUtils::ResourceManager& manager);

  void CreateBatchManagerBuffers(VkDevice device, VkPhysicalDevice physicalDevice);
  void CreateDescriptorSets(VkDevice device, VkPhysicalDevice physicalDevice);
  void RebuildBatchManager(VkDevice device, VkPhysicalDevice physicalDevice);
  void ChangeTexture(VkDevice device, VkPhysicalDevice physicalDevice, int idx, std::string& path);

 public:
  std::vector<MiniBatch> m_miniBatchList;
  std::vector<BasicVertex> m_accumulatedVertices;
  std::vector<uint32_t> m_accumulatedIndices;
  size_t m_accumulatedVertexSize = 0;
  size_t m_accumulatedIndexSize = 0;
  uint64_t m_accumulatedIndirectOffset = 0;
  uint32_t m_accumulatedMeshIndex = 0;
  uint64_t m_accmulatedVertexOffset = 0;
  uint64_t m_accmulatedIndexOffset = 0;

  // Descriptor Set
  VkDescriptorSetLayout m_batchSetLayout;
  std::vector<VkDescriptorSet> m_batchSets;

  // Material ID
  std::vector<ObjectID> m_objectIDList;
  GpuBuffer m_objectIDBuffer;

  // Model
  std::vector<Transform> m_trasformList;
  std::vector<Transform> m_transforms[MAX_FRAME_DRAWS];
  std::vector<GpuBuffer> m_transformListBuffer;

  // Material
  std::vector<GpuImage> m_diffuseImages;
  std::vector<VkDescriptorSet> m_textureIdList;
  VkSampler m_sampler;
  GpuImage oldImage;

  // Indirect Draw Call
  GpuBuffer m_indirectDrawCommandBuffer;

  // Bounding Box
  std::vector<AABB> m_boundingBoxList;
  GpuBuffer m_boundingBoxListBuffer;
  // - Each bounding box array (for visible bounding box)
  std::vector<AABBBufferList> m_boundingBoxBufferList;

  // Test
  std::vector<GpuBuffer> m_bbVertexBuffers;
  std::vector<GpuBuffer> m_bbIndexBuffers;

  // Meshlet buffers
  std::vector<GpuBuffer> m_positionBuffers;
  std::vector<GpuBuffer> m_meshletBuffers;
  std::vector<GpuBuffer> m_meshletVerticesBuffers;
  std::vector<GpuBuffer> m_meshletTrianglesBuffers;

  VkDescriptorSetLayout m_meshletSetLayout;
  std::vector<VkDescriptorSet> m_meshletSets;


  /*
    Ray Tracing
  */
  std::vector<Mesh> m_meshes;
  std::vector<RayTracingVertex> m_allMeshVertices;
  GpuBuffer m_verticesBuffer;

  std::vector<uint32_t> m_allMeshIndices;
  GpuBuffer m_indicesBuffer;

  std::vector<InstanceOffset> m_instanceOffsets;
  GpuBuffer m_instanceOffsetBuffer;

 public:
  void CreateTransformListBuffers(VkDevice device, VkPhysicalDevice physicalDevice);
  void CreateIndirectDrawBuffers(VkDevice device, VkPhysicalDevice physicalDevice);
  void CreateObjectIDBuffers(VkDevice device, VkPhysicalDevice physicalDevice);
  void CreateTextureBuffers(VkDevice device, VkPhysicalDevice physicalDevice);
  void CreateBoundingBoxBuffers(VkDevice device, VkPhysicalDevice physicalDevice);
  void CreateRaytracingBuffers(VkDevice device, VkPhysicalDevice physicalDevice);
};

#define g_BatchManager BatchManager::Get()

static void AddDataToMiniBatch(std::vector<MiniBatch>& miniBatches, VkUtils::ResourceManager& manager, const Mesh& mesh,
                               bool flag = false) {
  // 현재 메쉬 데이터 크기 계산
  size_t vertexDataSize = mesh.vertexCount * sizeof(BasicVertex);
  size_t indexDataSize = mesh.indexCount * sizeof(uint32_t);
  size_t totalDataSize = vertexDataSize + indexDataSize;

  // 만약 miniBatches가 비어 있으면 하나 생성
  if (s_accumulatedVertexSize == 0 && s_accumulatedIndexSize == 0) {
    miniBatches.emplace_back();
  }

  MiniBatch* currentBatch = &miniBatches.back();

  // Draw Command 생성 및 추가
  VkDrawIndexedIndirectCommand drawCommand{};
  drawCommand.indexCount = mesh.indexCount;
  drawCommand.instanceCount = 1;
  drawCommand.firstIndex = s_accumulatedIndexSize / sizeof(uint32_t);
  drawCommand.vertexOffset = s_accumulatedVertexSize / sizeof(BasicVertex);  // It's not byte offset, Just Index Offset
  drawCommand.firstInstance = s_accumulatedMeshIndex++;

  currentBatch->m_drawIndexedCommands.push_back(drawCommand);

  // 누적된 데이터에 현재 메쉬 추가
  s_accumulatedVertices.insert(s_accumulatedVertices.end(), mesh.vertices.begin(), mesh.vertices.end());
  s_accumulatedIndices.insert(s_accumulatedIndices.end(), mesh.indices.begin(), mesh.indices.end());
  std::cout << mesh.indices.size() << std::endl;

  s_accumulatedVertexSize += vertexDataSize;
  s_accumulatedIndexSize += indexDataSize;

  // 누적된 크기가 3MB를 초과하면 새로운 mini-batch 생성 또는 현재까지 아무것도 생성하지 않았을 경우, 마지막에 생성
  if (s_accumulatedVertexSize + s_accumulatedIndexSize > MAX_BATCH_SIZE || flag) {
    // 새로운 mini-batch 생성
    manager.CreateVertexBuffer(s_accumulatedVertexSize, &currentBatch->m_vertexBufferMemory, &currentBatch->m_vertexBuffer,
                               s_accumulatedVertices.data());
    manager.CreateIndexBuffer(s_accumulatedIndexSize, &currentBatch->m_indexBufferMemory, &currentBatch->m_indexBuffer,
                              s_accumulatedIndices.data());

    currentBatch->m_currentVertexOffset = s_accumulatedVertexSize;
    currentBatch->m_currentIndexOffset = s_accumulatedIndexSize;
    currentBatch->m_currentBatchSize = s_accumulatedVertexSize + s_accumulatedIndexSize;

    currentBatch->m_indirectCommandsOffset = s_accumulatedIndirectOffset;
    s_accumulatedIndirectOffset +=
        static_cast<uint64_t>(currentBatch->m_drawIndexedCommands.size() * sizeof(VkDrawIndexedIndirectCommand));

    std::cout << "New mini-batch created with size: " << currentBatch->m_currentBatchSize << " bytes." << std::endl;

    // 누적된 데이터 초기화
    s_accumulatedVertices.clear();
    s_accumulatedIndices.clear();
    s_accumulatedVertexSize = 0;
    s_accumulatedIndexSize = 0;
    s_accumulatedMeshIndex = 0;

  } else {
    std::cout << "Accumulating mesh data. Current accumulated size: " << s_accumulatedVertexSize + s_accumulatedIndexSize << " bytes."
              << std::endl;
  }
}

static void FlushMiniBatch(std::vector<MiniBatch>& miniBatches, VkUtils::ResourceManager& manager) {
  if (s_accumulatedVertexSize == 0 && s_accumulatedIndexSize == 0) return;

  MiniBatch* currentBatch = &miniBatches.back();

  manager.CreateVertexBuffer(s_accumulatedVertexSize, &currentBatch->m_vertexBufferMemory, &currentBatch->m_vertexBuffer,
                             s_accumulatedVertices.data());
  manager.CreateIndexBuffer(s_accumulatedIndexSize, &currentBatch->m_indexBufferMemory, &currentBatch->m_indexBuffer,
                            s_accumulatedIndices.data());

  currentBatch->m_currentVertexOffset = s_accumulatedVertexSize;
  currentBatch->m_currentIndexOffset = s_accumulatedIndexSize;
  currentBatch->m_currentBatchSize = s_accumulatedVertexSize + s_accumulatedIndexSize;

  currentBatch->m_indirectCommandsOffset = s_accumulatedIndirectOffset;

  std::cout << "Flushed mini-batch with size: " << currentBatch->m_currentBatchSize << " bytes." << std::endl;
  std::cout << "Flushed mini-batch with fefefe: " << currentBatch->m_indirectCommandsOffset << " bytes." << std::endl;

  // 누적된 데이터 초기화
  s_accumulatedVertices.clear();
  s_accumulatedIndices.clear();
  s_accumulatedVertexSize = 0;
  s_accumulatedIndexSize = 0;
  s_accumulatedMeshIndex = 0;
}