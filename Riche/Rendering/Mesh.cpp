#include "Mesh.h"

void Mesh::Initialize(std::vector<BasicVertex> _vertices, std::vector<uint32_t> _indices)
{
	vertices = _vertices;
	indices = _indices;
	vertexCount = vertices.size();
	indexCount = indices.size();
}

void Mesh::Cleanup()
{
}

uint32_t Mesh::GetIndexCount()
{
	return indexCount;
}

glm::mat4& Mesh::GetModel()
{
	return m_Model;
}
