#pragma once

#include "Components.h"
#include "Mesh.h"
#include "VkUtils/ResourceManager.h"

static const uint32_t MAX_BATCH_SIZE = 3 * 1024 * 1024; // 3MB
static std::vector<BasicVertex> accumulatedVertices;
static std::vector<uint32_t> accumulatedIndices;
static size_t accumulatedVertexSize = 0;
static size_t accumulatedIndexSize = 0;
static uint32_t meshIndex = 0;

struct DrawIndexedIndirectCPU
{
    VkDrawIndexedIndirectCommand drawIndexedCommand;
    uint32_t padding[3];
};

struct MiniBatch
{
    VkBuffer m_VertexBuffer = nullptr;
    VkDeviceMemory m_VertexBufferMemory;
    VkBuffer m_IndexBuffer = nullptr;
    VkDeviceMemory m_IndexBufferMemory;

    uint32_t currentVertexOffset = 0;
    uint32_t currentIndexOffset = 0;
    uint32_t currentBatchSize = 0;

    std::vector<VkDrawIndexedIndirectCommand> m_DrawIndexedCommands;
};

static void AddDataToMiniBatch(std::vector<MiniBatch>& miniBatches, VkUtils::ResourceManager& manager, const Mesh& mesh, bool flag = false) {

    // 현재 메쉬 데이터 크기 계산
    size_t vertexDataSize = mesh.vertexCount * sizeof(BasicVertex);
    size_t indexDataSize = mesh.indexCount * sizeof(uint32_t);
    size_t totalDataSize = vertexDataSize + indexDataSize;
    MiniBatch* currentBatch = nullptr;
    if (!miniBatches.empty()) {
        currentBatch = &miniBatches.back();
    }
    else {
        // mini-batch가 없으면 새로운 batch 생성
        MiniBatch newBatch;
        miniBatches.push_back(newBatch);
        currentBatch = &miniBatches.back();
    }

    // Draw Command 생성 및 추가
    VkDrawIndexedIndirectCommand drawCommand{};
    drawCommand.indexCount = mesh.indexCount;
    drawCommand.instanceCount = 1;
    drawCommand.firstIndex = accumulatedIndexSize / sizeof(uint32_t);
    drawCommand.vertexOffset = accumulatedVertexSize / sizeof(BasicVertex); // It's not byte offset, Just Index Offset
    drawCommand.firstInstance = meshIndex++;

    currentBatch->m_DrawIndexedCommands.push_back(drawCommand);

    // 누적된 데이터에 현재 메쉬 추가
    accumulatedVertices.insert(accumulatedVertices.end(), mesh.vertices.begin(), mesh.vertices.end());
    accumulatedIndices.insert(accumulatedIndices.end(), mesh.indices.begin(), mesh.indices.end());

    accumulatedVertexSize += vertexDataSize;
    accumulatedIndexSize += indexDataSize;

    // 누적된 크기가 3MB를 초과하면 새로운 mini-batch 생성 또는 현재까지 아무것도 생성하지 않았을 경우, 마지막에 생성
    if (accumulatedVertexSize + accumulatedIndexSize > MAX_BATCH_SIZE || flag)
    {
        // 새로운 mini-batch 생성
        MiniBatch miniBatch;
        manager.CreateVertexBuffer(accumulatedVertexSize, &miniBatch.m_VertexBufferMemory, &miniBatch.m_VertexBuffer, accumulatedVertices.data());
        manager.CreateIndexBuffer(accumulatedIndexSize, &miniBatch.m_IndexBufferMemory, &miniBatch.m_IndexBuffer, accumulatedIndices.data());

        miniBatch.currentVertexOffset = accumulatedVertexSize;
        miniBatch.currentIndexOffset = accumulatedIndexSize;
        miniBatch.currentBatchSize = accumulatedVertexSize + accumulatedIndexSize;

        miniBatches.push_back(miniBatch);

        std::cout << "New mini-batch created with size: " << miniBatch.currentBatchSize << " bytes." << std::endl;

        // 누적된 데이터 초기화
        accumulatedVertices.clear();
        accumulatedIndices.clear();
        accumulatedVertexSize = 0;
        accumulatedIndexSize = 0;
    }
    else {
        std::cout << "Accumulating mesh data. Current accumulated size: "
            << accumulatedVertexSize + accumulatedIndexSize << " bytes." << std::endl;
    }
}

static void FlushMiniBatch(std::vector<MiniBatch>& miniBatches, VkUtils::ResourceManager& manager) 
{
    if (accumulatedVertexSize == 0 && accumulatedIndexSize == 0) return;

    MiniBatch* currentBatch = nullptr;
    if (!miniBatches.empty()) {
        currentBatch = &miniBatches.back();
    }
    else {
        // mini-batch가 없으면 새로운 batch 생성
        MiniBatch newBatch;
        miniBatches.push_back(newBatch);
        currentBatch = &miniBatches.back();
    }
    manager.CreateVertexBuffer(accumulatedVertexSize, &currentBatch->m_VertexBufferMemory, &currentBatch->m_VertexBuffer, accumulatedVertices.data());
    manager.CreateIndexBuffer(accumulatedIndexSize, &currentBatch->m_IndexBufferMemory, &currentBatch->m_IndexBuffer, accumulatedIndices.data());

    currentBatch->currentVertexOffset = accumulatedVertexSize;
    currentBatch->currentIndexOffset = accumulatedIndexSize;
    currentBatch->currentBatchSize = accumulatedVertexSize + accumulatedIndexSize;

    std::cout << "Flushed mini-batch with size: " << currentBatch->currentBatchSize << " bytes." << std::endl;

    // 누적된 데이터 초기화
    accumulatedVertices.clear();
    accumulatedIndices.clear();
    accumulatedVertexSize = 0;
    accumulatedIndexSize = 0;
}