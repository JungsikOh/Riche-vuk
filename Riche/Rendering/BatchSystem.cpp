#include "BatchSystem.h"

void BatchManager::Update(VkDevice device) {}

void BatchManager::UpdateDescriptorSets(VkDevice device) {
  VkDescriptorBufferInfo transformUBOInfo = {};
  transformUBOInfo.buffer = g_BatchManager.m_transformListBuffer.buffer;  // Buffer to get data from
  transformUBOInfo.offset = 0;                                            // Position of start of data
  transformUBOInfo.range = g_BatchManager.m_transformListBuffer.size;     // size of data

  VkDescriptorBufferInfo indirectBufferInfo = {};
  indirectBufferInfo.buffer = g_BatchManager.m_indirectDrawCommandBuffer.buffer;  // Buffer to get data from
  indirectBufferInfo.offset = 0;                                                  // Position of start of data
  indirectBufferInfo.range = g_BatchManager.m_indirectDrawCommandBuffer.size;     // size of data

  VkDescriptorBufferInfo aabbIndirectInfo = {};
  aabbIndirectInfo.buffer = g_BatchManager.m_boundingBoxListBuffer.buffer;  // Buffer to get data from
  aabbIndirectInfo.offset = 0;                                              // Position of start of data
  aabbIndirectInfo.range = g_BatchManager.m_boundingBoxListBuffer.size;     // size of data

  VkDescriptorBufferInfo idUBOInfo = {};
  idUBOInfo.buffer = g_BatchManager.m_objectIDBuffer.buffer;  // Buffer to get data from
  idUBOInfo.offset = 0;                                       // Position of start of data
  idUBOInfo.range = g_BatchManager.m_objectIDBuffer.size;     // size of data

  VkUtils::DescriptorBuilder batchBuilder = VkUtils::DescriptorBuilder::Begin(&g_DescriptorLayoutCache, &g_DescriptorAllocator);
  batchBuilder.BindBuffer(0, &transformUBOInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
  batchBuilder.BindBuffer(1, &indirectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
  batchBuilder.BindBuffer(2, &aabbIndirectInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
  batchBuilder.BindBuffer(3, &idUBOInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);

  g_DescriptorManager.UpdateDescriptorSet(&batchBuilder, g_DescriptorManager.GetVkDescriptorSet("BATCH_ALL"));

  /*
   * Textures (for Lighting)
   */
  VkDeviceSize imageListSize = 0;
  for (GpuImage& image : m_diffuseImages) {
    imageListSize += image.size;
  }

  std::vector<VkDescriptorImageInfo> imageInfos;
  imageInfos.resize(m_diffuseImages.size());
  for (size_t i = 0; i < m_diffuseImages.size(); ++i) {
    imageInfos[i].imageView = m_diffuseImages[i].imageView;
    imageInfos[i].sampler = VK_NULL_HANDLE;
    imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

  VkUtils::DescriptorBuilder materialBuilder = VkUtils::DescriptorBuilder::Begin(&g_DescriptorLayoutCache, &g_DescriptorAllocator);
  materialBuilder.BindImage(0, imageInfos.data(), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_ALL, true, imageInfos.size());
  g_DescriptorManager.UpdateDescriptorSet(&materialBuilder, g_DescriptorManager.GetVkDescriptorSet("DiffuseTextureList"));
}

void BatchManager::Cleanup(VkDevice device) {
  vkDestroyBuffer(device, m_indirectDrawCommandBuffer.buffer, nullptr);
  vkFreeMemory(device, m_indirectDrawCommandBuffer.memory, nullptr);

  vkDestroyBuffer(device, m_boundingBoxListBuffer.buffer, nullptr);
  vkFreeMemory(device, m_boundingBoxListBuffer.memory, nullptr);

  vkDestroyBuffer(device, m_transformListBuffer.buffer, nullptr);
  vkFreeMemory(device, m_transformListBuffer.memory, nullptr);

  vkDestroyBuffer(device, m_objectIDBuffer.buffer, nullptr);
  vkFreeMemory(device, m_objectIDBuffer.memory, nullptr);

  for (int i = 0; i < m_diffuseImages.size(); ++i) {
    vkDestroyImageView(device, m_diffuseImages[i].imageView, nullptr);
    vkDestroyImage(device, m_diffuseImages[i].image, nullptr);
    vkFreeMemory(device, m_diffuseImages[i].memory, nullptr);
  }

  vkDestroyBuffer(device, m_instanceOffsetBuffer.buffer, nullptr);
  vkFreeMemory(device, m_instanceOffsetBuffer.memory, nullptr);

  vkDestroyBuffer(device, m_verticesBuffer.buffer, nullptr);
  vkFreeMemory(device, m_verticesBuffer.memory, nullptr);

  vkDestroyBuffer(device, m_indicesBuffer.buffer, nullptr);
  vkFreeMemory(device, m_indicesBuffer.memory, nullptr);

  for (int i = 0; i < m_boundingBoxBufferList.size(); ++i) {
    vkDestroyBuffer(device, m_boundingBoxBufferList[i].vertexBuffer, nullptr);
    vkFreeMemory(device, m_boundingBoxBufferList[i].vertexBufferMemory, nullptr);

    vkDestroyBuffer(device, m_boundingBoxBufferList[i].indexBuffer, nullptr);
    vkFreeMemory(device, m_boundingBoxBufferList[i].indexBufferMemory, nullptr);
  }
}

void BatchManager::AddDataToMiniBatch(std::vector<MiniBatch>& miniBatches, VkUtils::ResourceManager& manager, const Mesh& mesh,
                                      bool flag) {
  // 현재 메쉬 데이터 크기 계산
  size_t vertexDataSize = mesh.vertexCount * sizeof(BasicVertex);
  size_t indexDataSize = mesh.indexCount * sizeof(uint32_t);
  size_t totalDataSize = vertexDataSize + indexDataSize;

  // 만약 miniBatches가 비어 있으면 하나 생성
  if (m_accumulatedVertexSize == 0 && m_accumulatedIndexSize == 0) {
    miniBatches.emplace_back();
  }

  MiniBatch* currentBatch = &miniBatches.back();

  // Draw Command 생성 및 추가
  VkDrawIndexedIndirectCommand drawCommand{};
  drawCommand.indexCount = mesh.indexCount;
  drawCommand.instanceCount = 1;
  drawCommand.firstIndex = m_accumulatedIndexSize / sizeof(uint32_t);
  drawCommand.vertexOffset = m_accumulatedVertexSize / sizeof(BasicVertex);  // It's not byte offset, Just Index Offset
  drawCommand.firstInstance = m_accumulatedMeshIndex++;

  currentBatch->m_drawIndexedCommands.push_back(drawCommand);

  // 누적된 데이터에 현재 메쉬 추가
  m_accumulatedVertices.insert(m_accumulatedVertices.end(), mesh.vertices.begin(), mesh.vertices.end());
  m_accumulatedIndices.insert(m_accumulatedIndices.end(), mesh.indices.begin(), mesh.indices.end());
  std::cout << mesh.indices.size() << std::endl;

  m_accumulatedVertexSize += vertexDataSize;
  m_accumulatedIndexSize += indexDataSize;

  // 누적된 크기가 3MB를 초과하면 새로운 mini-batch 생성 또는 현재까지 아무것도 생성하지 않았을 경우, 마지막에 생성
  if (m_accumulatedVertexSize + m_accumulatedIndexSize > MAX_BATCH_SIZE || flag) {
    // 새로운 mini-batch 생성
    manager.CreateVertexBuffer(m_accumulatedVertexSize, &currentBatch->m_vertexBufferMemory, &currentBatch->m_vertexBuffer,
                               m_accumulatedVertices.data());
    manager.CreateIndexBuffer(m_accumulatedIndexSize, &currentBatch->m_indexBufferMemory, &currentBatch->m_indexBuffer,
                              m_accumulatedIndices.data());

    currentBatch->m_currentVertexOffset = m_accumulatedVertexSize;
    currentBatch->m_currentIndexOffset = m_accumulatedIndexSize;
    currentBatch->m_currentBatchSize = m_accumulatedVertexSize + m_accumulatedIndexSize;

    currentBatch->m_indirectCommandsOffset = m_accumulatedIndirectOffset;
    m_accumulatedIndirectOffset +=
        static_cast<uint64_t>(currentBatch->m_drawIndexedCommands.size() * sizeof(VkDrawIndexedIndirectCommand));

    std::cout << "New mini-batch created with size: " << currentBatch->m_currentBatchSize << " bytes." << std::endl;

    // 누적된 데이터 초기화
    m_accumulatedVertices = {};
    m_accumulatedIndices = {};
    m_accumulatedVertexSize = 0;
    m_accumulatedIndexSize = 0;
    m_accumulatedMeshIndex = 0;

  } else {
    std::cout << "Accumulating mesh data. Current accumulated size: " << m_accumulatedVertexSize + m_accumulatedIndexSize << " bytes."
              << std::endl;
  }
}

void BatchManager::FlushMiniBatch(std::vector<MiniBatch>& miniBatches, VkUtils::ResourceManager& manager) {
  if (m_accumulatedVertexSize == 0 && m_accumulatedIndexSize == 0) return;

  MiniBatch* currentBatch = &miniBatches.back();

  manager.CreateVertexBuffer(m_accumulatedVertexSize, &currentBatch->m_vertexBufferMemory, &currentBatch->m_vertexBuffer,
                             m_accumulatedVertices.data());
  manager.CreateIndexBuffer(m_accumulatedIndexSize, &currentBatch->m_indexBufferMemory, &currentBatch->m_indexBuffer,
                            m_accumulatedIndices.data());

  currentBatch->m_currentVertexOffset = m_accumulatedVertexSize;
  currentBatch->m_currentIndexOffset = m_accumulatedIndexSize;
  currentBatch->m_currentBatchSize = m_accumulatedVertexSize + m_accumulatedIndexSize;

  currentBatch->m_indirectCommandsOffset = m_accumulatedIndirectOffset;
  m_accumulatedIndirectOffset +=
      static_cast<uint64_t>(currentBatch->m_drawIndexedCommands.size() * sizeof(VkDrawIndexedIndirectCommand));

  std::cout << "Flushed mini-batch with size: " << currentBatch->m_currentBatchSize << " bytes." << std::endl;
  std::cout << "Flushed mini-batch with fefefe: " << currentBatch->m_indirectCommandsOffset << " bytes." << std::endl;

  // 누적된 데이터 초기화
  m_accumulatedVertices = {};
  m_accumulatedIndices = {};
  m_accumulatedVertexSize = 0;
  m_accumulatedIndexSize = 0;
  m_accumulatedMeshIndex = 0;
}

void BatchManager::CreateBatchManagerBuffers(VkDevice device, VkPhysicalDevice physicalDevice) {
  CreateTransformListBuffers(device, physicalDevice);
  CreateIndirectDrawBuffers(device, physicalDevice);
  CreateBoundingBoxBuffers(device, physicalDevice);
  CreateObjectIDBuffers(device, physicalDevice);
  CreateRaytracingBuffers(device, physicalDevice);
}

void BatchManager::CreateDescriptorSets(VkDevice device, VkPhysicalDevice physicalDevice) {
  VkDescriptorBufferInfo transformUBOInfo = {};
  transformUBOInfo.buffer = g_BatchManager.m_transformListBuffer.buffer;  // Buffer to get data from
  transformUBOInfo.offset = 0;                                            // Position of start of data
  transformUBOInfo.range = g_BatchManager.m_transformListBuffer.size;     // size of data

  VkDescriptorBufferInfo indirectBufferInfo = {};
  indirectBufferInfo.buffer = g_BatchManager.m_indirectDrawCommandBuffer.buffer;  // Buffer to get data from
  indirectBufferInfo.offset = 0;                                                  // Position of start of data
  indirectBufferInfo.range = g_BatchManager.m_indirectDrawCommandBuffer.size;     // size of data

  VkDescriptorBufferInfo aabbIndirectInfo = {};
  aabbIndirectInfo.buffer = g_BatchManager.m_boundingBoxListBuffer.buffer;  // Buffer to get data from
  aabbIndirectInfo.offset = 0;                                              // Position of start of data
  aabbIndirectInfo.range = g_BatchManager.m_boundingBoxListBuffer.size;     // size of data

  VkDescriptorBufferInfo idUBOInfo = {};
  idUBOInfo.buffer = g_BatchManager.m_objectIDBuffer.buffer;  // Buffer to get data from
  idUBOInfo.offset = 0;                                       // Position of start of data
  idUBOInfo.range = g_BatchManager.m_objectIDBuffer.size;     // size of data

  VkUtils::DescriptorBuilder batchBuilder = VkUtils::DescriptorBuilder::Begin(&g_DescriptorLayoutCache, &g_DescriptorAllocator);
  batchBuilder.BindBuffer(0, &transformUBOInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
  batchBuilder.BindBuffer(1, &indirectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
  batchBuilder.BindBuffer(2, &aabbIndirectInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
  batchBuilder.BindBuffer(3, &idUBOInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);

  g_DescriptorManager.AddDescriptorSet(&batchBuilder, "BATCH_ALL");

  /*
   * Textures (for Lighting)
   */
  VkDeviceSize imageListSize = 0;
  for (GpuImage& image : m_diffuseImages) {
    imageListSize += image.size;
  }

  std::vector<VkDescriptorImageInfo> imageInfos;
  imageInfos.resize(m_diffuseImages.size());
  for (size_t i = 0; i < m_diffuseImages.size(); ++i) {
    imageInfos[i].imageView = m_diffuseImages[i].imageView;
    imageInfos[i].sampler = VK_NULL_HANDLE;
    imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

  VkUtils::DescriptorBuilder materialBuilder = VkUtils::DescriptorBuilder::Begin(&g_DescriptorLayoutCache, &g_DescriptorAllocator);
  materialBuilder.BindImage(0, imageInfos.data(), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_ALL, true, imageInfos.size());
  g_DescriptorManager.AddDescriptorSet(&materialBuilder, "DiffuseTextureList", true);
}

void BatchManager::RebuildBatchManager(VkDevice device, VkPhysicalDevice physicalDevice) {
  vkDestroyBuffer(device, m_indirectDrawCommandBuffer.buffer, nullptr);
  vkFreeMemory(device, m_indirectDrawCommandBuffer.memory, nullptr);

  vkDestroyBuffer(device, m_boundingBoxListBuffer.buffer, nullptr);
  vkFreeMemory(device, m_boundingBoxListBuffer.memory, nullptr);

  vkDestroyBuffer(device, m_transformListBuffer.buffer, nullptr);
  vkFreeMemory(device, m_transformListBuffer.memory, nullptr);

  vkDestroyBuffer(device, m_objectIDBuffer.buffer, nullptr);
  vkFreeMemory(device, m_objectIDBuffer.memory, nullptr);

  m_instanceOffsets.clear();
  vkDestroyBuffer(device, m_instanceOffsetBuffer.buffer, nullptr);
  vkFreeMemory(device, m_instanceOffsetBuffer.memory, nullptr);

  vkDestroyBuffer(device, m_verticesBuffer.buffer, nullptr);
  vkFreeMemory(device, m_verticesBuffer.memory, nullptr);

  vkDestroyBuffer(device, m_indicesBuffer.buffer, nullptr);
  vkFreeMemory(device, m_indicesBuffer.memory, nullptr);

  CreateBatchManagerBuffers(device, physicalDevice);
  UpdateDescriptorSets(device);
}

void BatchManager::CreateTransformListBuffers(VkDevice device, VkPhysicalDevice physicalDevice) {
  void* pData = nullptr;

  // Transform
  VkDeviceSize transformBufferSize = static_cast<uint64_t>(m_trasformList.size() * sizeof(Transform));
  m_transformListBuffer.size = transformBufferSize;
  VkUtils::CreateBuffer(device, physicalDevice, transformBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_transformListBuffer.buffer,
                        &m_transformListBuffer.memory);

  vkMapMemory(device, m_transformListBuffer.memory, 0, transformBufferSize, 0, &pData);
  memcpy(pData, m_trasformList.data(), (size_t)transformBufferSize);
  vkUnmapMemory(device, m_transformListBuffer.memory);
}

void BatchManager::CreateIndirectDrawBuffers(VkDevice device, VkPhysicalDevice physicalDevice) {
  void* pData = nullptr;

  VkDeviceSize indirectBufferSize = 0;
  std::vector<VkDrawIndexedIndirectCommand> flattenCommands;
  for (int i = 0; i < m_miniBatchList.size(); ++i) {
    indirectBufferSize +=
        static_cast<uint64_t>(sizeof(VkDrawIndexedIndirectCommand) * m_miniBatchList[i].m_drawIndexedCommands.size());
    flattenCommands.insert(flattenCommands.end(), m_miniBatchList[i].m_drawIndexedCommands.begin(),
                           m_miniBatchList[i].m_drawIndexedCommands.end());
  }
  VkUtils::CreateBuffer(device, physicalDevice, indirectBufferSize,
                        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        &m_indirectDrawCommandBuffer.buffer, &m_indirectDrawCommandBuffer.memory);

  vkMapMemory(device, m_indirectDrawCommandBuffer.memory, 0, indirectBufferSize, 0, &pData);
  memcpy(pData, flattenCommands.data(), (size_t)indirectBufferSize);
  vkUnmapMemory(device, m_indirectDrawCommandBuffer.memory);
  m_indirectDrawCommandBuffer.size = indirectBufferSize;
}

void BatchManager::CreateObjectIDBuffers(VkDevice device, VkPhysicalDevice physicalDevice) {
  void* pData = nullptr;

  VkDeviceSize idBufferSize = static_cast<uint64_t>(m_objectIDList.size() * sizeof(ObjectID));
  m_objectIDBuffer.size = idBufferSize;
  VkUtils::CreateBuffer(device, physicalDevice, idBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_objectIDBuffer.buffer,
                        &m_objectIDBuffer.memory);

  vkMapMemory(device, m_objectIDBuffer.memory, 0, idBufferSize, 0, &pData);
  memcpy(pData, m_objectIDList.data(), (size_t)idBufferSize);
  vkUnmapMemory(device, m_objectIDBuffer.memory);
}

void BatchManager::CreateTextureBuffers(VkDevice device, VkPhysicalDevice physicalDevice) {}

void BatchManager::CreateBoundingBoxBuffers(VkDevice device, VkPhysicalDevice physicalDevice) {
  void* pData = nullptr;

  VkDeviceSize aabbBufferSize = static_cast<uint64_t>(sizeof(AABB) * m_boundingBoxList.size());
  m_boundingBoxListBuffer.size = aabbBufferSize;
  VkUtils::CreateBuffer(device, physicalDevice, aabbBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_boundingBoxListBuffer.buffer,
                        &m_boundingBoxListBuffer.memory);

  vkMapMemory(device, m_boundingBoxListBuffer.memory, 0, aabbBufferSize, 0, &pData);
  memcpy(pData, m_boundingBoxList.data(), (size_t)aabbBufferSize);
  vkUnmapMemory(device, m_boundingBoxListBuffer.memory);
}

void BatchManager::CreateRaytracingBuffers(VkDevice device, VkPhysicalDevice physicalDevice) {
  void* pData = nullptr;

  /*
   * Vertices + Indices + Offset Buffer
   */
  m_verticesBuffer.size = static_cast<uint64_t>(m_allMeshVertices.size() * sizeof(RayTracingVertex));
  VkUtils::CreateBuffer(device, physicalDevice, m_verticesBuffer.size,
                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_verticesBuffer.buffer,
                        &m_verticesBuffer.memory, true);

  vkMapMemory(device, m_verticesBuffer.memory, 0, m_verticesBuffer.size, 0, &pData);
  memcpy(pData, m_allMeshVertices.data(), (size_t)m_verticesBuffer.size);
  vkUnmapMemory(device, m_verticesBuffer.memory);

  m_indicesBuffer.size = static_cast<uint64_t>(m_allMeshIndices.size() * sizeof(uint32_t));
  VkUtils::CreateBuffer(device, physicalDevice, m_indicesBuffer.size,
                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_indicesBuffer.buffer,
                        &m_indicesBuffer.memory, true);

  vkMapMemory(device, m_indicesBuffer.memory, 0, m_indicesBuffer.size, 0, &pData);
  memcpy(pData, m_allMeshIndices.data(), (size_t)m_indicesBuffer.size);
  vkUnmapMemory(device, m_indicesBuffer.memory);

  uint32_t vertexOffset = 0;
  uint32_t indexOffset = 0;
  for (auto& mesh : m_meshes) {
    InstanceOffset offset;
    offset.vertexOffset = vertexOffset;
    offset.indicesOffset = indexOffset;

    m_instanceOffsets.push_back(offset);

    vertexOffset += mesh.vertices.size();
    indexOffset += mesh.indices.size();
  }

  m_instanceOffsetBuffer.size = static_cast<uint64_t>(m_instanceOffsets.size() * sizeof(InstanceOffset));
  VkUtils::CreateBuffer(device, physicalDevice, m_instanceOffsetBuffer.size,
                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_instanceOffsetBuffer.buffer,
                        &m_instanceOffsetBuffer.memory, true);

  vkMapMemory(device, m_instanceOffsetBuffer.memory, 0, m_instanceOffsetBuffer.size, 0, &pData);
  memcpy(pData, m_instanceOffsets.data(), (size_t)m_instanceOffsetBuffer.size);
  vkUnmapMemory(device, m_instanceOffsetBuffer.memory);
}
