#pragma once

#include "Components.h"
#include "Mesh.h"
#include "VkUtils/ResourceManager.h"

static const uint32_t MAX_BATCH_SIZE = 3 * 1024 * 1024;  // 3MB
static std::vector<BasicVertex> accumulatedVertices;
static std::vector<uint32_t> accumulatedIndices;
static size_t accumulatedVertexSize = 0;
static size_t accumulatedIndexSize = 0;
static uint64_t accumulatedIndirectOffset = 0;
static uint32_t accumulatedMeshIndex = 0;

struct MiniBatch {
  VkBuffer m_vertexBuffer = nullptr;
  VkDeviceMemory m_vertexBufferMemory;
  VkBuffer m_indexBuffer = nullptr;
  VkDeviceMemory m_indexBufferMemory;

  uint32_t m_currentVertexOffset = 0;
  uint32_t m_currentIndexOffset = 0;
  uint32_t m_currentBatchSize = 0;

  uint64_t m_indirectCommandsOffset = 0;
  std::vector<VkDrawIndexedIndirectCommand> m_drawIndexedCommands;
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