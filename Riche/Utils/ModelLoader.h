#pragma once

#include "Rendering/VulkanRenderer.h"
#include "Rendering/Components.h"
#include "Rendering/Mesh.h"
#include "Singleton.h"
#include "ThreadPool.h"

static entt::registry g_Registry;

static bool loadObjModel(const std::string& filepath, std::vector<Mesh>& outMeshes , float scale = 1.0f) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  // OBJ 파일 로드
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str(), nullptr);
  if (!warn.empty()) std::cout << "[TinyObjLoader Warning] " << warn << std::endl;
  if (!err.empty()) std::cerr << "[TinyObjLoader Error] " << err << std::endl;
  if (!ret) return false;

  std::vector<std::future<Mesh>> futures;
  futures.reserve(shapes.size());

  for (size_t i = 0; i < shapes.size(); ++i) {
    auto future = g_ThreadPool.Submit([&, i]() -> Mesh {
      Mesh data;
      for (size_t f = 0; f < shapes[i].mesh.indices.size(); f++) {
        tinyobj::index_t idx = shapes[i].mesh.indices[f];
        BasicVertex vertex;

        // vertex.pos
        if (idx.vertex_index >= 0) {
          vertex.pos = {attrib.vertices[3 * idx.vertex_index + 0], attrib.vertices[3 * idx.vertex_index + 1],
                        attrib.vertices[3 * idx.vertex_index + 2]};
        }

        // vertex.normal
        if (idx.normal_index >= 0) {
          vertex.normal = {attrib.normals[3 * idx.normal_index + 0], attrib.normals[3 * idx.normal_index + 1],
                           attrib.normals[3 * idx.normal_index + 2]};
        }

        // vertex.tex
        if (idx.texcoord_index >= 0) {
          vertex.tex = {attrib.texcoords[2 * idx.texcoord_index + 0], attrib.texcoords[2 * idx.texcoord_index + 1]};
        }

        data.vertices.push_back(std::move(vertex));
        // index (단순히 f를 쓰거나, unique 처리)
        data.indices.push_back(static_cast<uint32_t>(f));
      }
      return data;
    });

    futures.push_back(std::move(future));
  }

  size_t initObjectIDSize = g_BatchManager.m_meshIDList.size();
  for (int i = 0; i < futures.size(); ++i) {
    auto& f = futures[i];

    Mesh partial = f.get();
    partial.vertexCount = static_cast<uint32_t>(partial.vertices.size());
    partial.indexCount = static_cast<uint32_t>(partial.indices.size());

    AddDataToMiniBatch(g_BatchManager.m_miniBatchList, g_ResourceManager, partial);

    entt::entity object = g_Registry.create();
    ObjectID _id;
    _id.handle = static_cast<uint64_t>(i + initObjectIDSize);
    g_BatchManager.m_meshIDList.push_back(_id);
    g_Registry.emplace<ObjectID>(object, _id);

    Transform _transfrom = {};
    _transfrom.startTransform = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
    _transfrom.currentTransform = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
    g_BatchManager.m_trasformList.push_back(_transfrom);
    g_Registry.emplace<Transform>(object, _transfrom);

    AABB _aabb = ComputeAABB(partial.vertices);
    g_BatchManager.m_boundingBoxList.push_back(_aabb);
    g_Registry.emplace<AABB>(object, _aabb);

    outMeshes.push_back(partial);
  }

  return true;
}