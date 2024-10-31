#include "Mesh.h"

void Mesh::Initialize(VkDevice device, uint32_t vertexSize, uint32_t indexSize)
{
	m_Device = device;
	vertexCount = vertexSize;
	indexCount = indexSize;
}

void Mesh::Cleanup()
{
	vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
	vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);
	vkDestroyBuffer(m_Device, m_IndexBuffer, nullptr);
	vkFreeMemory(m_Device, m_IndexBufferMemory, nullptr);
}

VkBuffer& Mesh::GetVkVertexBuffer()
{
	return m_VertexBuffer;
}

VkDeviceMemory& Mesh::GetVkVertexDeviceMemory()
{
	return m_VertexBufferMemory;
}

VkBuffer& Mesh::GetVkIndexBuffer()
{
	return m_IndexBuffer;
}

VkDeviceMemory& Mesh::GetVkIndexDeviceMemory()
{
	return m_IndexBufferMemory;
}

uint32_t Mesh::GetIndexCount()
{
	return indexCount;
}
