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

struct MiniBatch
{
    VkBuffer m_vertexBuffer = nullptr;
    VkDeviceMemory m_vertexBufferMemory;
    VkBuffer m_indexBuffer = nullptr;
    VkDeviceMemory m_indexBufferMemory;

    uint32_t currentVertexOffset = 0;
    uint32_t currentIndexOffset = 0;
    uint32_t currentBatchSize = 0;

    std::vector<VkDrawIndexedIndirectCommand> m_drawIndexedCommands;
};

static void AddDataToMiniBatch(std::vector<MiniBatch>& miniBatches, VkUtils::ResourceManager& manager, const Mesh& mesh, bool flag = false) {

    // ���� �޽� ������ ũ�� ���
    size_t vertexDataSize = mesh.vertexCount * sizeof(BasicVertex);
    size_t indexDataSize = mesh.indexCount * sizeof(uint32_t);
    size_t totalDataSize = vertexDataSize + indexDataSize;
    MiniBatch* currentBatch = nullptr;
    if (!miniBatches.empty()) {
        currentBatch = &miniBatches.back();
    }
    else {
        // mini-batch�� ������ ���ο� batch ����
        MiniBatch newBatch;
        miniBatches.push_back(newBatch);
        currentBatch = &miniBatches.back();
    }

    // Draw Command ���� �� �߰�
    VkDrawIndexedIndirectCommand drawCommand{};
    drawCommand.indexCount = mesh.indexCount;
    drawCommand.instanceCount = 1;
    drawCommand.firstIndex = accumulatedIndexSize / sizeof(uint32_t);
    drawCommand.vertexOffset = accumulatedVertexSize / sizeof(BasicVertex); // It's not byte offset, Just Index Offset
    drawCommand.firstInstance = meshIndex++;

    currentBatch->m_drawIndexedCommands.push_back(drawCommand);

    // ������ �����Ϳ� ���� �޽� �߰�
    accumulatedVertices.insert(accumulatedVertices.end(), mesh.vertices.begin(), mesh.vertices.end());
    accumulatedIndices.insert(accumulatedIndices.end(), mesh.indices.begin(), mesh.indices.end());

    accumulatedVertexSize += vertexDataSize;
    accumulatedIndexSize += indexDataSize;

    // ������ ũ�Ⱑ 3MB�� �ʰ��ϸ� ���ο� mini-batch ���� �Ǵ� ������� �ƹ��͵� �������� �ʾ��� ���, �������� ����
    if (accumulatedVertexSize + accumulatedIndexSize > MAX_BATCH_SIZE || flag)
    {
        // ���ο� mini-batch ����
        MiniBatch miniBatch;
        manager.CreateVertexBuffer(accumulatedVertexSize, &miniBatch.m_vertexBufferMemory, &miniBatch.m_vertexBuffer, accumulatedVertices.data());
        manager.CreateIndexBuffer(accumulatedIndexSize, &miniBatch.m_indexBufferMemory, &miniBatch.m_indexBuffer, accumulatedIndices.data());

        miniBatch.currentVertexOffset = accumulatedVertexSize;
        miniBatch.currentIndexOffset = accumulatedIndexSize;
        miniBatch.currentBatchSize = accumulatedVertexSize + accumulatedIndexSize;

        miniBatches.push_back(miniBatch);

        std::cout << "New mini-batch created with size: " << miniBatch.currentBatchSize << " bytes." << std::endl;

        // ������ ������ �ʱ�ȭ
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
        // mini-batch�� ������ ���ο� batch ����
        MiniBatch newBatch;
        miniBatches.push_back(newBatch);
        currentBatch = &miniBatches.back();
    }
    manager.CreateVertexBuffer(accumulatedVertexSize, &currentBatch->m_vertexBufferMemory, &currentBatch->m_vertexBuffer, accumulatedVertices.data());
    manager.CreateIndexBuffer(accumulatedIndexSize, &currentBatch->m_indexBufferMemory, &currentBatch->m_indexBuffer, accumulatedIndices.data());

    currentBatch->currentVertexOffset = accumulatedVertexSize;
    currentBatch->currentIndexOffset = accumulatedIndexSize;
    currentBatch->currentBatchSize = accumulatedVertexSize + accumulatedIndexSize;

    std::cout << "Flushed mini-batch with size: " << currentBatch->currentBatchSize << " bytes." << std::endl;

    // ������ ������ �ʱ�ȭ
    accumulatedVertices.clear();
    accumulatedIndices.clear();
    accumulatedVertexSize = 0;
    accumulatedIndexSize = 0;
}