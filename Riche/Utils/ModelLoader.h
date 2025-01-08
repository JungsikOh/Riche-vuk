#pragma once

#include "Rendering/Components.h"
#include "Rendering/Mesh.h"
#include "Singleton.h"
#include "ThreadPool.h"

struct MeshData {
  std::vector<float> positions;
  std::vector<float> normals;
  std::vector<float> texcoords;
  std::vector<uint32_t> indices;
};

// �ؽ� �Լ� ���� (unordered_map ����� ���� ��)namespace std {
template <>
struct std::hash<BasicVertex> {
  size_t operator()(const BasicVertex& vertex) const {
    size_t h1 = std::hash<float>()(vertex.pos.x) ^ std::hash<float>()(vertex.pos.y) ^ std::hash<float>()(vertex.pos.z);
    size_t h2 = std::hash<float>()(vertex.normal.x) ^ std::hash<float>()(vertex.normal.y) ^ std::hash<float>()(vertex.normal.z);
    size_t h3 = std::hash<float>()(vertex.tex.x) ^ std::hash<float>()(vertex.tex.y);
    return h1 ^ (h2 << 1) ^ (h3 << 1);
  }
};

static void ProcessShape(const tinyobj::attrib_t& attrib, const tinyobj::shape_t& shape, Mesh& outMesh) {

  for (size_t f = 0; f < shape.mesh.indices.size(); f++) {
    tinyobj::index_t idx = shape.mesh.indices[f];
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

    outMesh.vertices.push_back(std::move(vertex));
    // index (�ܼ��� f�� ���ų�, unique ó��)
    outMesh.indices.push_back(static_cast<uint32_t>(f));
  }
}

static bool loadObjModel(const std::string& filepath, std::vector<Mesh>& outMeshes) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  // OBJ ���� �ε�
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str(), nullptr);
  if (!warn.empty()) std::cout << "[TinyObjLoader Warning] " << warn << std::endl;
  if (!err.empty()) std::cerr << "[TinyObjLoader Error] " << err << std::endl;
  if (!ret) return false;

  std::vector<std::future<Mesh>> futures;
  futures.reserve(shapes.size());

  for (size_t i = 0; i < shapes.size(); ++i) {
    auto future = g_ThreadPool.Submit([&, i]() -> Mesh {
      Mesh data;
      ProcessShape(attrib, shapes[i], data);
      return data;
    });

    futures.push_back(std::move(future));
  }

  // Combine results
  for (auto& f : futures) {
    Mesh partial = f.get();
    partial.vertexCount = static_cast<uint32_t>(partial.vertices.size());
    partial.indexCount = static_cast<uint32_t>(partial.indices.size());
    outMeshes.push_back(std::move(partial));
  }

  //
  // Single Thread
  //
  // ��� shape�� ��ȸ�ϸ� Mesh ����
  // for (const auto& shape : shapes) {
  //  Mesh mesh;
  //  std::unordered_map<BasicVertex, uint32_t> uniqueVertices;  // �ߺ� ���ſ� ��

  //  size_t indexOffset = 0;
  //  for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
  //    int fv = shape.mesh.num_face_vertices[f];  // face�� ���ؽ� ��

  //    for (int v = 0; v < fv; v++) {
  //      tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
  //      BasicVertex vertex{};

  //      // vertex.pos
  //      if (idx.vertex_index >= 0) {
  //        vertex.pos = {attrib.vertices[3 * idx.vertex_index + 0], attrib.vertices[3 * idx.vertex_index + 1],
  //                      attrib.vertices[3 * idx.vertex_index + 2]};
  //      } else {
  //        // ���� �ε��� ó��...
  //        // (TinyObjLoader���� �̹� ��ȯ���� ���� ����)
  //      }

  //      // vertex.normal
  //      if (idx.normal_index >= 0) {
  //        vertex.normal = {attrib.normals[3 * idx.normal_index + 0], attrib.normals[3 * idx.normal_index + 1],
  //                         attrib.normals[3 * idx.normal_index + 2]};
  //      }

  //      // vertex.tex
  //      if (idx.texcoord_index >= 0) {
  //        vertex.tex = {attrib.texcoords[2 * idx.texcoord_index + 0], attrib.texcoords[2 * idx.texcoord_index + 1]};
  //      }

  //      // �ߺ� üũ
  //      if (uniqueVertices.count(vertex) == 0) {
  //        uniqueVertices[vertex] = static_cast<uint32_t>(mesh.vertices.size());
  //        mesh.vertices.push_back(vertex);
  //      }
  //      mesh.indices.push_back(uniqueVertices[vertex]);
  //    }

  //    indexOffset += fv;
  //  }

  //  // Mesh ���� ����
  //  mesh.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
  //  mesh.indexCount = static_cast<uint32_t>(mesh.indices.size());

  //  // outMeshes�� �߰�
  //  outMeshes.push_back(std::move(mesh));
  //}

  return true;
}

static bool loadGltfModel(const std::string& filepath, std::vector<Mesh>& outMeshes) {
  tinygltf::TinyGLTF loader;
  tinygltf::Model model;
  std::string err, warn;

  // glTF �ε�: ASCII(.gltf) / Binary(.glb) ��� ����
  // �ʿ信 ���� LoadASCIIFromFile vs LoadBinaryFromFile ���
  bool ret = false;
  if (filepath.size() > 5 && (filepath.substr(filepath.size() - 5) == ".glb" || filepath.substr(filepath.size() - 4) == ".glb")) {
    ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
  } else {
    ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
  }

  if (!warn.empty()) std::cout << "[TinyGLTF Warning] " << warn << std::endl;
  if (!err.empty()) std::cerr << "[TinyGLTF Error] " << err << std::endl;
  if (!ret) {
    std::cerr << "Failed to load glTF: " << filepath << std::endl;
    return false;
  }

  // glTF�� mesh���� ��ȸ
  // model.meshes -> �� mesh �ȿ� primitives�� ���� ��
  for (const auto& gltfMesh : model.meshes) {
    // �� primitive�� �ϳ��� sub-mesh�� ��
    for (const auto& primitive : gltfMesh.primitives) {
      // positions, normals, texcoords accessor index
      // (��: primitive.attributes["POSITION"], ["NORMAL"], ["TEXCOORD_0"] ��)
      // indices�� primitive.indices
      if (primitive.attributes.find("POSITION") == primitive.attributes.end()) {
        // POSITION�� ������ ��ŵ(�ʼ�)
        continue;
      }

      // accessor indices
      int posAccIndex = primitive.attributes.at("POSITION");
      int norAccIndex = -1;
      int texAccIndex = -1;

      if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
        norAccIndex = primitive.attributes.at("NORMAL");
      }
      if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
        texAccIndex = primitive.attributes.at("TEXCOORD_0");
      }

      // �ε��� ���� accessor
      int idxAccIndex = primitive.indices;  // -1 �̸� �ε��� ���۰� ���� ���� ����(��: ��ο� ���)
      if (idxAccIndex < 0) {
        // �ε����� ���� primitive��� ��ŵ/�߰� ���� �ʿ�
        continue;
      }

      // ���� accessor �ε�
      const tinygltf::Accessor& posAccessor = model.accessors[posAccIndex];
      const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
      const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];

      const tinygltf::Accessor& idxAccessor = model.accessors[idxAccIndex];
      const tinygltf::BufferView& idxView = model.bufferViews[idxAccessor.bufferView];
      const tinygltf::Buffer& idxBuffer = model.buffers[idxView.buffer];

      // ���, UV accessor�� ������ �ε�
      const tinygltf::Accessor* norAccessor = nullptr;
      const tinygltf::BufferView* norView = nullptr;
      const tinygltf::Buffer* norBuf = nullptr;
      if (norAccIndex >= 0) {
        norAccessor = &model.accessors[norAccIndex];
        norView = &model.bufferViews[norAccessor->bufferView];
        norBuf = &model.buffers[norView->buffer];
      }

      const tinygltf::Accessor* texAccessor = nullptr;
      const tinygltf::BufferView* texView = nullptr;
      const tinygltf::Buffer* texBuf = nullptr;
      if (texAccIndex >= 0) {
        texAccessor = &model.accessors[texAccIndex];
        texView = &model.bufferViews[texAccessor->bufferView];
        texBuf = &model.buffers[texView->buffer];
      }

      // CPU�� mesh ������
      Mesh mesh;
      std::unordered_map<BasicVertex, uint32_t> uniqueVertices;

      // �ε��� ����
      size_t indexCount = idxAccessor.count;

      // glTF �ε����� ���� UNSIGNED_SHORT(0x1403) or UNSIGNED_INT(0x1405)
      // accessor.componentType�� ����
      const unsigned char* idxDataPtr = idxBuffer.data.data() + idxView.byteOffset + idxAccessor.byteOffset;

      for (size_t i = 0; i < indexCount; i++) {
        uint32_t indexVal = 0;
        if (idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT) {
          const uint16_t* buf = reinterpret_cast<const uint16_t*>(idxDataPtr);
          indexVal = static_cast<uint32_t>(buf[i]);
        } else if (idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT) {
          const uint32_t* buf = reinterpret_cast<const uint32_t*>(idxDataPtr);
          indexVal = buf[i];
        } else {
          // ��Ÿ ���� ó�� �ʿ�(��: byte, short ��). ���⼭�� ����
          continue;
        }

        // position
        BasicVertex vertex{};
        {
          // posAccessor.type == TINYGLTF_TYPE_VEC3
          size_t posByteOffset = posView.byteOffset + posAccessor.byteOffset + (indexVal * posAccessor.ByteStride(posView));
          const float* posData = reinterpret_cast<const float*>(posBuffer.data.data() + posByteOffset);
          vertex.pos = glm::vec3(posData[0], posData[1], posData[2]);
        }

        // normal
        if (norAccessor) {
          size_t norByteOffset = norView->byteOffset + norAccessor->byteOffset + (indexVal * norAccessor->ByteStride(*norView));
          const float* norData = reinterpret_cast<const float*>(norBuf->data.data() + norByteOffset);
          vertex.normal = glm::vec3(norData[0], norData[1], norData[2]);
        }

        // tex
        if (texAccessor) {
          size_t texByteOffset = texView->byteOffset + texAccessor->byteOffset + (indexVal * texAccessor->ByteStride(*texView));
          const float* uvData = reinterpret_cast<const float*>(texBuf->data.data() + texByteOffset);
          vertex.tex = glm::vec2(uvData[0], uvData[1]);
        }

        // �ߺ� �˻�
        if (uniqueVertices.find(vertex) == uniqueVertices.end()) {
          uint32_t newIndex = static_cast<uint32_t>(mesh.vertices.size());
          uniqueVertices[vertex] = newIndex;
          mesh.vertices.push_back(vertex);
          mesh.indices.push_back(newIndex);
        } else {
          mesh.indices.push_back(uniqueVertices[vertex]);
        }
      }  // for (indexCount)

      // mesh ���� ����
      mesh.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
      mesh.indexCount = static_cast<uint32_t>(mesh.indices.size());

      outMeshes.push_back(std::move(mesh));
    }  // for (primitive)
  }  // for (meshes)

  return true;
}