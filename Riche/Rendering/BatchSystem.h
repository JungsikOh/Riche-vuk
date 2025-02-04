#pragma once

#include "Components.h"
#include "Mesh.h"
#include "Utils/Boundingbox.h"
#include "Utils/Singleton.h"
#include "VkUtils/DescriptorManager.h"
#include "VkUtils/ResourceManager.h"

static const uint32_t MAX_BATCH_SIZE = 3 * 1024 * 1024;  // 3MB
static std::vector<BasicVertex> accumulatedVertices;
static std::vector<uint32_t> accumulatedIndices;
static size_t accumulatedVertexSize = 0;
static size_t accumulatedIndexSize = 0;
static uint64_t accumulatedIndirectOffset = 0;
static uint32_t accumulatedMeshIndex = 0;

struct MiniBatch {
  VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory m_vertexBufferMemory;
  VkBuffer m_indexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory m_indexBufferMemory;

  std::vector<VkImage> m_imageGPU;
  VkDeviceMemory m_imageGPUMemory;
  std::vector<VkImageView> m_imageViewGPU;

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

struct BatchManager : public Singleton<BatchManager> {
  std::vector<MiniBatch> m_miniBatchList;
  std::vector<ObjectID> m_meshIDList;
  VkBuffer m_objectIDListBuffer;
  VkDeviceMemory m_objectIDListBufferMemory;

  // Model
  std::vector<Transform> m_trasformList;
  VkBuffer m_trasformListBuffer;
  VkDeviceMemory m_trasformListBufferMemory;

  // Material
  std::vector<VkImage> m_diffuseImageList;
  std::vector<VkImageView> m_diffuseImageViewList;
  std::vector<VkDeviceMemory> m_diffuseImageListMemory;
  std::vector<VkDeviceSize> m_diffuseImageListSize;

  // -- Indirect Draw Call
  VkBuffer m_indirectDrawBuffer;
  VkDeviceMemory m_indirectDrawBufferMemory;
  VkDeviceSize m_indirectDrawCommandSize;

  // -- Bounding Box
  std::vector<AABB> m_boundingBoxList;
  VkBuffer m_boundingBoxListBuffer;
  VkDeviceMemory m_boundingBoxListBufferMemory;
  std::vector<AABBBufferList> m_boundingBoxBufferList;

  void Cleanup(VkDevice device) {
    vkDestroyBuffer(device, m_indirectDrawBuffer, nullptr);
    vkFreeMemory(device, m_indirectDrawBufferMemory, nullptr);

    vkDestroyBuffer(device, m_boundingBoxListBuffer, nullptr);
    vkFreeMemory(device, m_boundingBoxListBufferMemory, nullptr);

    vkDestroyBuffer(device, m_trasformListBuffer, nullptr);
    vkFreeMemory(device, m_trasformListBufferMemory, nullptr);

    vkDestroyBuffer(device, m_objectIDListBuffer, nullptr);
    vkFreeMemory(device, m_objectIDListBufferMemory, nullptr);

    for (int i = 0; i < m_diffuseImageList.size(); ++i) {
      vkDestroyImageView(device, m_diffuseImageViewList[i], nullptr);
      vkDestroyImage(device, m_diffuseImageList[i], nullptr);
      vkFreeMemory(device, m_diffuseImageListMemory[i], nullptr);
    }

    for (int i = 0; i < m_boundingBoxBufferList.size(); ++i) {
      vkDestroyBuffer(device, m_boundingBoxBufferList[i].vertexBuffer, nullptr);
      vkFreeMemory(device, m_boundingBoxBufferList[i].vertexBufferMemory, nullptr);

      vkDestroyBuffer(device, m_boundingBoxBufferList[i].indexBuffer, nullptr);
      vkFreeMemory(device, m_boundingBoxBufferList[i].indexBufferMemory, nullptr);
    }
  }
};

static void AddDataToMiniBatch(std::vector<MiniBatch>& miniBatches, VkUtils::ResourceManager& manager, const Mesh& mesh,
                               bool flag = false) {
  // 현재 메쉬 데이터 크기 계산
  size_t vertexDataSize = mesh.vertexCount * sizeof(BasicVertex);
  size_t indexDataSize = mesh.indexCount * sizeof(uint32_t);
  size_t totalDataSize = vertexDataSize + indexDataSize;

  // 만약 miniBatches가 비어 있으면 하나 생성
  if (miniBatches.empty()) {
    miniBatches.emplace_back();
  }
  MiniBatch* currentBatch = &miniBatches.back();

  // Draw Command 생성 및 추가
  VkDrawIndexedIndirectCommand drawCommand{};
  drawCommand.indexCount = mesh.indexCount;
  drawCommand.instanceCount = 1;
  drawCommand.firstIndex = accumulatedIndexSize / sizeof(uint32_t);
  drawCommand.vertexOffset = accumulatedVertexSize / sizeof(BasicVertex);  // It's not byte offset, Just Index Offset
  drawCommand.firstInstance = accumulatedMeshIndex++;

  currentBatch->m_drawIndexedCommands.push_back(drawCommand);

  // 누적된 데이터에 현재 메쉬 추가
  accumulatedVertices.insert(accumulatedVertices.end(), mesh.vertices.begin(), mesh.vertices.end());
  accumulatedIndices.insert(accumulatedIndices.end(), mesh.indices.begin(), mesh.indices.end());

  accumulatedVertexSize += vertexDataSize;
  accumulatedIndexSize += indexDataSize;

  // 누적된 크기가 3MB를 초과하면 새로운 mini-batch 생성 또는 현재까지 아무것도 생성하지 않았을 경우, 마지막에 생성
  if (accumulatedVertexSize + accumulatedIndexSize > MAX_BATCH_SIZE || flag) {
    // 새로운 mini-batch 생성
    manager.CreateVertexBuffer(accumulatedVertexSize, &currentBatch->m_vertexBufferMemory, &currentBatch->m_vertexBuffer,
                               accumulatedVertices.data());
    manager.CreateIndexBuffer(accumulatedIndexSize, &currentBatch->m_indexBufferMemory, &currentBatch->m_indexBuffer,
                              accumulatedIndices.data());

    currentBatch->m_currentVertexOffset = accumulatedVertexSize;
    currentBatch->m_currentIndexOffset = accumulatedIndexSize;
    currentBatch->m_currentBatchSize = accumulatedVertexSize + accumulatedIndexSize;

    currentBatch->m_indirectCommandsOffset = accumulatedIndirectOffset;
    accumulatedIndirectOffset +=
        static_cast<uint64_t>(currentBatch->m_drawIndexedCommands.size() * sizeof(VkDrawIndexedIndirectCommand));

    std::cout << "New mini-batch created with size: " << currentBatch->m_currentBatchSize << " bytes." << std::endl;

    miniBatches.emplace_back();

    // 누적된 데이터 초기화
    accumulatedVertices.clear();
    accumulatedIndices.clear();
    accumulatedVertexSize = 0;
    accumulatedIndexSize = 0;
    accumulatedMeshIndex = 0;

  } else {
    std::cout << "Accumulating mesh data. Current accumulated size: " << accumulatedVertexSize + accumulatedIndexSize << " bytes."
              << std::endl;
  }
}

static void FlushMiniBatch(std::vector<MiniBatch>& miniBatches, VkUtils::ResourceManager& manager) {
  if (accumulatedVertexSize == 0 && accumulatedIndexSize == 0) return;

  MiniBatch* currentBatch = &miniBatches.back();

  manager.CreateVertexBuffer(accumulatedVertexSize, &currentBatch->m_vertexBufferMemory, &currentBatch->m_vertexBuffer,
                             accumulatedVertices.data());
  manager.CreateIndexBuffer(accumulatedIndexSize, &currentBatch->m_indexBufferMemory, &currentBatch->m_indexBuffer,
                            accumulatedIndices.data());

  currentBatch->m_currentVertexOffset = accumulatedVertexSize;
  currentBatch->m_currentIndexOffset = accumulatedIndexSize;
  currentBatch->m_currentBatchSize = accumulatedVertexSize + accumulatedIndexSize;

  currentBatch->m_indirectCommandsOffset = accumulatedIndirectOffset;

  std::cout << "Flushed mini-batch with size: " << currentBatch->m_currentBatchSize << " bytes." << std::endl;


  // 누적된 데이터 초기화
  accumulatedVertices.clear();
  accumulatedIndices.clear();
  accumulatedVertexSize = 0;
  accumulatedIndexSize = 0;
  accumulatedMeshIndex = 0;
}

#define g_BatchManager BatchManager::Get()