#pragma once

#include "Rendering/Mesh.h"
#include "Singleton.h"

static bool loadObjModel(const std::string& filepath, Mesh& outMesh) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str());
  if (!warn.empty()) std::cout << "[TinyObjLoader Warning] " << warn << std::endl;
  if (!err.empty()) std::cerr << "[TinyObjLoader Error] " << err << std::endl;
  if (!ret) return false;

  for (const auto& shape : shapes) {
    for (const auto& index : shape.mesh.indices) {
      BasicVertex vertex{};

      if (index.vertex_index >= 0) {
        vertex.pos = {attrib.vertices[3 * index.vertex_index + 0], attrib.vertices[3 * index.vertex_index + 1],
                      attrib.vertices[3 * index.vertex_index + 2]};
      }

      if (index.normal_index >= 0) {
        vertex.normal = {attrib.normals[3 * index.normal_index + 0], attrib.normals[3 * index.normal_index + 1],
                         attrib.normals[3 * index.normal_index + 2]};
      }

      if (index.texcoord_index >= 0) {
        vertex.tex = {attrib.texcoords[2 * index.texcoord_index + 0], attrib.texcoords[2 * index.texcoord_index + 1]};
      }

      outMesh.vertices.push_back(vertex);
      outMesh.indices.push_back(static_cast<uint32_t>(outMesh.vertices.size() - 1));
    }
  }

  outMesh.vertexCount = static_cast<uint32_t>(outMesh.vertices.size());
  outMesh.indexCount = static_cast<uint32_t>(outMesh.indices.size());
  return true;
}