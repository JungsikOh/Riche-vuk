#pragma once

#include "Rendering/Components.h"
#include "Rendering/Mesh.h"
#include "Rendering/VulkanRenderer.h"
#include "Singleton.h"
#include "ThreadPool.h"

static entt::registry g_Registry;

static bool loadObjModel(VkDevice device, const std::string& filepath, const std::string& objName, std::vector<Mesh>& outMeshes,
                         float scale = 1.0f) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  // OBJ ���� �ε�
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, (filepath + objName).c_str(), filepath.c_str());
  if (!warn.empty()) std::cout << "[TinyObjLoader Warning] " << warn << std::endl;
  if (!err.empty()) std::cerr << "[TinyObjLoader Error] " << err << std::endl;
  if (!ret) return false;

  std::vector<std::future<Mesh>> futures;
  futures.reserve(shapes.size());

  for (size_t i = 0; i < shapes.size(); ++i) {
    auto future = g_ThreadPool.Submit([&, i]() -> Mesh {
      Mesh data;
      if (!shapes[i].mesh.material_ids.empty()) {
        data.materialID = static_cast<uint32_t>(shapes[i].mesh.material_ids[0]);
      }

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
        // index (�ܼ��� f�� ���ų�, unique ó��)
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
    _id.materialID = partial.materialID;
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

  for (size_t i = 0; i < materials.size(); ++i) {
    const tinyobj::material_t& mat = materials[i];

    if (!mat.diffuse_texname.empty()) {
      std::string texturePath = filepath + mat.diffuse_texname;
      std::replace(texturePath.begin(), texturePath.end(), '\\', '/');

      VkImage _image;
      VkDeviceMemory _imageMemory;
      VkDeviceSize _imageSize;
      VkImageView _imageView;

      g_ResourceManager.CreateTexture(texturePath, &_imageMemory, &_image, &_imageSize);
      VkUtils::CreateImageView(device, _image, &_imageView, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

      g_BatchManager.m_diffuseImageList.push_back(_image);
      g_BatchManager.m_diffuseImageViewList.push_back(_imageView);
      g_BatchManager.m_diffuseImageListMemory.push_back(_imageMemory);
      g_BatchManager.m_diffuseImageListSize.push_back(_imageSize);
    }
  }

  return true;
}

static bool loadGltfModel(VkDevice device, const std::string& filepath, const std::string& gltfName, std::vector<Mesh>& outMeshes,
                          float scale = 1.0f) {
  tinygltf::TinyGLTF loader;
  tinygltf::Model model;
  std::string warn, err;

  // glTF �ε� (.gltf�� ��� ASCII, .glb�� ��� Binary �δ� ���)
  // ����) .gltf �����̶� ����
  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, (filepath + gltfName));
  // ���� .glb���:
  // bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, (filepath + gltfName));

  if (!warn.empty()) {
    std::cout << "[tinygltf Warning] " << warn << std::endl;
  }
  if (!err.empty()) {
    std::cerr << "[tinygltf Error] " << err << std::endl;
  }
  if (!ret) {
    std::cerr << "Failed to load glTF: " << filepath + gltfName << std::endl;
    return false;
  }

  // glTF�� ���� ���� Mesh�� ���� �� �ְ�, �� Mesh�� ���� Primitive�� ���� �� �ֽ��ϴ�.
  // ������ Primitive�� OBJ�� shape�� �����ϰ� ����Ͽ� Mesh�� ��ȯ�Ѵٰ� �����մϴ�.
  std::vector<std::future<Mesh>> futures;
  futures.reserve(model.meshes.size());  // �����δ� mesh �� * primitive ����ŭ ���� �� ����

  // glTF�� �� mesh -> �� primitive �� ���Ͽ� �����͸� ����
  for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex) {
    const tinygltf::Mesh& gltfMesh = model.meshes[meshIndex];

    for (size_t primIndex = 0; primIndex < gltfMesh.primitives.size(); ++primIndex) {
      const tinygltf::Primitive& primitive = gltfMesh.primitives[primIndex];

      // ������ Ǯ�� �۾��� ����
      auto future = g_ThreadPool.Submit([&, meshIndex, primIndex]() -> Mesh {
        Mesh data;

        // materialID ���� (glTF������ primitive.material�� �ε���)
        // ���� ��� -1�̰ų�, unsigned(-1) ���� ���� �� ������ üũ
        if (primitive.material >= 0 && primitive.material < static_cast<int>(model.materials.size())) {
          data.materialID = static_cast<int>(primitive.material);
        } else {
          data.materialID = -1;  // �⺻��
        }

        // �ε��� ���� ó��
        std::vector<uint32_t> localIndices;
        if (primitive.indices >= 0 && primitive.indices < static_cast<int>(model.accessors.size())) {
          const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
          const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
          const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

          const uint8_t* dataPtr = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
          size_t indexCount = indexAccessor.count;
          localIndices.reserve(indexCount);

          // �ε��� Ÿ�� (unsigned short, unsigned int ��)
          if (indexAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT) {
            for (size_t i = 0; i < indexCount; ++i) {
              uint16_t idx = reinterpret_cast<const uint16_t*>(dataPtr)[i];
              localIndices.push_back(static_cast<uint32_t>(idx));
            }
          } else if (indexAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT) {
            for (size_t i = 0; i < indexCount; ++i) {
              uint32_t idx = reinterpret_cast<const uint32_t*>(dataPtr)[i];
              localIndices.push_back(idx);
            }
          } else if (indexAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE) {
            for (size_t i = 0; i < indexCount; ++i) {
              uint8_t idx = reinterpret_cast<const uint8_t*>(dataPtr)[i];
              localIndices.push_back(static_cast<uint32_t>(idx));
            }
          } else {
            // �� �� ������ ���⼭ ���� ó�� Ȥ�� ����
            std::cerr << "[tinygltf] Unsupported index component type.\n";
          }
        } else {
          // �ε����� ���� ���, primitive�� �ﰢ�� ���¶��
          // vertices�� �׳� ������� ���� ���� (draco ���� ���� ������� ���� �ܼ� ����)
          // �ʿ� �� primitive.mode (POINTS, TRIANGLES ��)�� ���� ó��
          // ���⼭�� TRIANGLES�� �����ϰ� Accessor�κ��� ������ŭ push_back
          // �Ǵ�, ���� ���� ...
        }

        // POSITION, NORMAL, TEXCOORD_0 �� Attribute ó��
        // glTF���� Attribute �̸�: "POSITION", "NORMAL", "TEXCOORD_0" ��
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texcoords;

        // glTF�� attributes�� std::map<std::string, int> ����
        auto itPos = primitive.attributes.find("POSITION");
        if (itPos != primitive.attributes.end()) {
          const tinygltf::Accessor& accessor = model.accessors[itPos->second];
          const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
          const tinygltf::Buffer& buffer = model.buffers[view.buffer];

          const size_t accessorCount = accessor.count;
          positions.reserve(accessorCount);

          const uint8_t* dataPtr = buffer.data.data() + view.byteOffset + accessor.byteOffset;
          // stride ���
          size_t stride = accessor.ByteStride(view);
          if (!stride) {
            // �⺻ stride ���
            stride = tinygltf::GetNumComponentsInType(accessor.type) * tinygltf::GetComponentSizeInBytes(accessor.componentType);
          }

          for (size_t i = 0; i < accessorCount; ++i) {
            const float* elem = reinterpret_cast<const float*>(dataPtr + stride * i);
            glm::vec3 pos = glm::vec3(elem[0], elem[1], elem[2]) * scale;
            positions.push_back(pos);
          }
        }

        auto itNorm = primitive.attributes.find("NORMAL");
        if (itNorm != primitive.attributes.end()) {
          const tinygltf::Accessor& accessor = model.accessors[itNorm->second];
          const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
          const tinygltf::Buffer& buffer = model.buffers[view.buffer];

          const size_t accessorCount = accessor.count;
          normals.reserve(accessorCount);

          const uint8_t* dataPtr = buffer.data.data() + view.byteOffset + accessor.byteOffset;
          size_t stride = accessor.ByteStride(view);
          if (!stride) {
            stride = tinygltf::GetNumComponentsInType(accessor.type) * tinygltf::GetComponentSizeInBytes(accessor.componentType);
          }

          for (size_t i = 0; i < accessorCount; ++i) {
            const float* elem = reinterpret_cast<const float*>(dataPtr + stride * i);
            glm::vec3 nor = glm::vec3(elem[0], elem[1], elem[2]);
            normals.push_back(nor);
          }
        }

        auto itTex = primitive.attributes.find("TEXCOORD_0");
        if (itTex != primitive.attributes.end()) {
          const tinygltf::Accessor& accessor = model.accessors[itTex->second];
          const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
          const tinygltf::Buffer& buffer = model.buffers[view.buffer];

          const size_t accessorCount = accessor.count;
          texcoords.reserve(accessorCount);

          const uint8_t* dataPtr = buffer.data.data() + view.byteOffset + accessor.byteOffset;
          size_t stride = accessor.ByteStride(view);
          if (!stride) {
            stride = tinygltf::GetNumComponentsInType(accessor.type) * tinygltf::GetComponentSizeInBytes(accessor.componentType);
          }

          for (size_t i = 0; i < accessorCount; ++i) {
            const float* elem = reinterpret_cast<const float*>(dataPtr + stride * i);
            glm::vec2 uv = glm::vec2(elem[0], elem[1]);
            texcoords.push_back(uv);
          }
        }

        // vertex / index ���� ����
        // tinyobjloader�� ���ÿ� ����ϰ� BasicVertex�� ��� data.vertices�� �ִ´�.
        // �ε����� �����ϴ� ��� localIndices�� ���� �ﰢ��(�Ǵ� �� �� ���)�� ���� �Ҵ�
        // �ε����� ������ positions.size()��ŭ ���� push_back �� �� ����.
        if (!localIndices.empty()) {
          data.indices.reserve(localIndices.size());
          data.vertices.reserve(positions.size());  // rough

          // �켱 ���� ������ ��� BasicVertex ���·� ����
          for (size_t i = 0; i < positions.size(); ++i) {
            BasicVertex v{};
            v.pos = positions[i];
            v.normal = (i < normals.size()) ? normals[i] : glm::vec3(0, 0, 0);
            v.tex = (i < texcoords.size()) ? texcoords[i] : glm::vec2(0, 0);
            data.vertices.push_back(v);
          }

          // �ε��� �Ҵ�
          for (auto idx : localIndices) {
            data.indices.push_back(idx);
          }
        } else {
          // �ε����� ���� ��. (drawArrays ���)
          data.vertices.reserve(positions.size());

          for (size_t i = 0; i < positions.size(); ++i) {
            BasicVertex v{};
            v.pos = positions[i];
            v.normal = (i < normals.size()) ? normals[i] : glm::vec3(0, 0, 0);
            v.tex = (i < texcoords.size()) ? texcoords[i] : glm::vec2(0, 0);
            data.vertices.push_back(v);
            data.indices.push_back(static_cast<uint32_t>(i));
          }
        }

        return data;
      });

      futures.push_back(std::move(future));
    }
  }

  // future ��� ���� ��, ���� ����(Entt entity ����, MiniBatch ��� ��)
  size_t initObjectIDSize = g_BatchManager.m_meshIDList.size();
  for (auto& f : futures) {
    Mesh partial = f.get();

    partial.vertexCount = static_cast<uint32_t>(partial.vertices.size());
    partial.indexCount = static_cast<uint32_t>(partial.indices.size());

    // ���ҽ� �Ŵ����� ���� ���� ���� ����(���ε�) ��
    AddDataToMiniBatch(g_BatchManager.m_miniBatchList, g_ResourceManager, partial);

    // ��ƼƼ ���� �� ���
    entt::entity object = g_Registry.create();

    ObjectID _id;
    _id.materialID = partial.materialID;
    g_BatchManager.m_meshIDList.push_back(_id);
    g_Registry.emplace<ObjectID>(object, _id);

    Transform _transform = {};
    _transform.startTransform = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
    _transform.currentTransform = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
    g_BatchManager.m_trasformList.push_back(_transform);
    g_Registry.emplace<Transform>(object, _transform);

    AABB _aabb = ComputeAABB(partial.vertices);  // ����� ���� AABB ��� �Լ�
    g_BatchManager.m_boundingBoxList.push_back(_aabb);
    g_Registry.emplace<AABB>(object, _aabb);

    outMeshes.push_back(std::move(partial));
  }

  // glTF�� materials�� ���� �ؽ�ó �� �ε�
  // glTF������ PBRMetallicRoughness ���� ��� �ؽ�ó index�� ���� �� ����
  // �Ʒ��� baseColorTexture�� �ε��ϴ� ����
  for (size_t i = 0; i < model.materials.size(); ++i) {
    const tinygltf::Material& mat = model.materials[i];

    // baseColorTexture�� ���� �ε��� Ȯ��
    if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
      int texIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
      if (texIndex < (int)model.textures.size()) {
        const tinygltf::Texture& texture = model.textures[texIndex];
        if (texture.source >= 0 && texture.source < (int)model.images.size()) {
          const tinygltf::Image& image = model.images[texture.source];

          std::string texturePath = image.uri;
          // �ܺ� ������ ���, filepath + texturePath ���� ���� ���� ��ΰ� �� ���� ����
          // ��ο� '\\' ���� ��� ���� �� �����Ƿ� ��ü
          std::replace(texturePath.begin(), texturePath.end(), '\\', '/');


          if (texturePath.find(":") == std::string::npos) {
            texturePath = filepath + texturePath;
          }

          VkImage _image;
          VkDeviceMemory _imageMemory;
          VkDeviceSize _imageSize;
          VkImageView _imageView;

          g_ResourceManager.CreateTexture(texturePath, &_imageMemory, &_image, &_imageSize);
          VkUtils::CreateImageView(device, _image, &_imageView, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

          g_BatchManager.m_diffuseImageList.push_back(_image);
          g_BatchManager.m_diffuseImageViewList.push_back(_imageView);
          g_BatchManager.m_diffuseImageListMemory.push_back(_imageMemory);
          g_BatchManager.m_diffuseImageListSize.push_back(_imageSize);
        }
      }
    }
  }

  std::cout << "mesh count: " << g_BatchManager.m_meshIDList.size() << std::endl;
  return true;
}

static bool loadSeqGltfModel(VkDevice device, const std::string& filepath, const std::string& gltfName, std::vector<Mesh>& outMeshes,
                          float scale = 1.0f) {
  tinygltf::TinyGLTF loader;
  tinygltf::Model model;
  std::string warn, err;

  // glTF �ε� (.gltf�� ��� ASCII, .glb�� ��� Binary �δ� ���)
  // ���⼭�� .gltf �����̶� ����
  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, (filepath + gltfName));
  // ���� .glb���:
  // bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, (filepath + gltfName));

  if (!warn.empty()) {
    std::cout << "[tinygltf Warning] " << warn << std::endl;
  }
  if (!err.empty()) {
    std::cerr << "[tinygltf Error] " << err << std::endl;
  }
  if (!ret) {
    std::cerr << "Failed to load glTF: " << filepath + gltfName << std::endl;
    return false;
  }

  // glTF�� ���� ���� Mesh�� ���� �� �ְ�, �� Mesh�� ���� Primitive�� ���� �� �ֽ��ϴ�.
  // �� Mesh�� �� Primitive�� ���������� ó���Ͽ� Mesh �����͸� �����մϴ�.
  for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex) {
    const tinygltf::Mesh& gltfMesh = model.meshes[meshIndex];

    for (size_t primIndex = 0; primIndex < gltfMesh.primitives.size(); ++primIndex) {
      const tinygltf::Primitive& primitive = gltfMesh.primitives[primIndex];

      Mesh data;

      // materialID ���� (glTF������ primitive.material�� �ε���)
      // ���� ��� -1 Ȥ�� unsigned(-1) ���� ���� �� ������ üũ
      if (primitive.material >= 0 && primitive.material < static_cast<int>(model.materials.size())) {
        data.materialID = static_cast<int>(primitive.material);
      } else {
        data.materialID = -1;  // �⺻��
      }

      // �ε��� ���� ó��
      std::vector<uint32_t> localIndices;
      if (primitive.indices >= 0 && primitive.indices < static_cast<int>(model.accessors.size())) {
        const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
        const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

        const uint8_t* dataPtr = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
        size_t indexCount = indexAccessor.count;
        localIndices.reserve(indexCount);

        // �ε��� Ÿ�� (unsigned short, unsigned int ��)
        if (indexAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT) {
          for (size_t i = 0; i < indexCount; ++i) {
            uint16_t idx = reinterpret_cast<const uint16_t*>(dataPtr)[i];
            localIndices.push_back(static_cast<uint32_t>(idx));
          }
        } else if (indexAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT) {
          for (size_t i = 0; i < indexCount; ++i) {
            uint32_t idx = reinterpret_cast<const uint32_t*>(dataPtr)[i];
            localIndices.push_back(idx);
          }
        } else if (indexAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE) {
          for (size_t i = 0; i < indexCount; ++i) {
            uint8_t idx = reinterpret_cast<const uint8_t*>(dataPtr)[i];
            localIndices.push_back(static_cast<uint32_t>(idx));
          }
        } else {
          std::cerr << "[tinygltf] Unsupported index component type.\n";
        }
      } else {
        // �ε����� ���� ���, ���� ��� TRIANGLES ����� positions.size() ��ŭ �ε����� ���� ����
      }

      // POSITION, NORMAL, TEXCOORD_0 �� Attribute ó��
      std::vector<glm::vec3> positions;
      std::vector<glm::vec3> normals;
      std::vector<glm::vec2> texcoords;

      // POSITION ó��
      auto itPos = primitive.attributes.find("POSITION");
      if (itPos != primitive.attributes.end()) {
        const tinygltf::Accessor& accessor = model.accessors[itPos->second];
        const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[view.buffer];

        const size_t accessorCount = accessor.count;
        positions.reserve(accessorCount);

        const uint8_t* dataPtr = buffer.data.data() + view.byteOffset + accessor.byteOffset;
        size_t stride = accessor.ByteStride(view);
        if (!stride) {
          stride = tinygltf::GetNumComponentsInType(accessor.type) * tinygltf::GetComponentSizeInBytes(accessor.componentType);
        }

        for (size_t i = 0; i < accessorCount; ++i) {
          const float* elem = reinterpret_cast<const float*>(dataPtr + stride * i);
          glm::vec3 pos = glm::vec3(elem[0], elem[1], elem[2]) * scale;
          positions.push_back(pos);
        }
      }

      // NORMAL ó��
      auto itNorm = primitive.attributes.find("NORMAL");
      if (itNorm != primitive.attributes.end()) {
        const tinygltf::Accessor& accessor = model.accessors[itNorm->second];
        const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[view.buffer];

        const size_t accessorCount = accessor.count;
        normals.reserve(accessorCount);

        const uint8_t* dataPtr = buffer.data.data() + view.byteOffset + accessor.byteOffset;
        size_t stride = accessor.ByteStride(view);
        if (!stride) {
          stride = tinygltf::GetNumComponentsInType(accessor.type) * tinygltf::GetComponentSizeInBytes(accessor.componentType);
        }

        for (size_t i = 0; i < accessorCount; ++i) {
          const float* elem = reinterpret_cast<const float*>(dataPtr + stride * i);
          glm::vec3 nor = glm::vec3(elem[0], elem[1], elem[2]);
          normals.push_back(nor);
        }
      }

      // TEXCOORD_0 ó��
      auto itTex = primitive.attributes.find("TEXCOORD_0");
      if (itTex != primitive.attributes.end()) {
        const tinygltf::Accessor& accessor = model.accessors[itTex->second];
        const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[view.buffer];

        const size_t accessorCount = accessor.count;
        texcoords.reserve(accessorCount);

        const uint8_t* dataPtr = buffer.data.data() + view.byteOffset + accessor.byteOffset;
        size_t stride = accessor.ByteStride(view);
        if (!stride) {
          stride = tinygltf::GetNumComponentsInType(accessor.type) * tinygltf::GetComponentSizeInBytes(accessor.componentType);
        }

        for (size_t i = 0; i < accessorCount; ++i) {
          const float* elem = reinterpret_cast<const float*>(dataPtr + stride * i);
          glm::vec2 uv = glm::vec2(elem[0], elem[1]);
          texcoords.push_back(uv);
        }
      }

      // ���� vertex / index ����
      if (!localIndices.empty()) {
        data.indices.reserve(localIndices.size());
        data.vertices.reserve(positions.size());  // rough

        for (size_t i = 0; i < positions.size(); ++i) {
          BasicVertex v{};
          v.pos = positions[i];
          v.normal = (i < normals.size()) ? normals[i] : glm::vec3(0, 0, 0);
          v.tex = (i < texcoords.size()) ? texcoords[i] : glm::vec2(0, 0);
          data.vertices.push_back(v);
        }

        for (auto idx : localIndices) {
          data.indices.push_back(idx);
        }
      } else {
        // �ε����� ���� ���, drawArrays ������� ó�� (positions.size() ��ŭ ���� �ε��� �Ҵ�)
        data.vertices.reserve(positions.size());
        for (size_t i = 0; i < positions.size(); ++i) {
          BasicVertex v{};
          v.pos = positions[i];
          v.normal = (i < normals.size()) ? normals[i] : glm::vec3(0, 0, 0);
          v.tex = (i < texcoords.size()) ? texcoords[i] : glm::vec2(0, 0);
          data.vertices.push_back(v);
          data.indices.push_back(static_cast<uint32_t>(i));
        }
      }

      // �� primitive���� ������ Mesh �����͸� �ٷ� ó��
      // (�ʿ��ϸ�, �� Mesh���� ��ƼƼ ����, �̴Ϲ�ġ ��� ���� ����)
      data.vertexCount = static_cast<uint32_t>(data.vertices.size());
      data.indexCount = static_cast<uint32_t>(data.indices.size());

      // ��: �̴Ϲ�ġ�� ������ �߰�
      AddDataToMiniBatch(g_BatchManager.m_miniBatchList, g_ResourceManager, data);

      // ��ƼƼ ���� �� ��� (����)
      entt::entity object = g_Registry.create();
      ObjectID _id;
      _id.materialID = data.materialID;
      g_BatchManager.m_meshIDList.push_back(_id);
      g_Registry.emplace<ObjectID>(object, _id);

      Transform _transform = {};
      _transform.startTransform = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
      _transform.currentTransform = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
      g_BatchManager.m_trasformList.push_back(_transform);
      g_Registry.emplace<Transform>(object, _transform);

      AABB _aabb = ComputeAABB(data.vertices);  // ����� ���� AABB ��� �Լ�
      g_BatchManager.m_boundingBoxList.push_back(_aabb);
      g_Registry.emplace<AABB>(object, _aabb);

      // outMeshes�� ������ Mesh ������ �߰�
      outMeshes.push_back(std::move(data));
    }
  }

  // glTF�� materials�� ���� �ؽ�ó �� �ε� (baseColorTexture�� �ε��ϴ� ����)
  for (size_t i = 0; i < model.materials.size(); ++i) {
    const tinygltf::Material& mat = model.materials[i];

    if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
      int texIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
      if (texIndex < static_cast<int>(model.textures.size())) {
        const tinygltf::Texture& texture = model.textures[texIndex];
        if (texture.source >= 0 && texture.source < static_cast<int>(model.images.size())) {
          const tinygltf::Image& image = model.images[texture.source];

          std::string texturePath = image.uri;
          std::replace(texturePath.begin(), texturePath.end(), '\\', '/');
          if (texturePath.find(":") == std::string::npos) {
            texturePath = filepath + texturePath;
          }

          VkImage _image;
          VkDeviceMemory _imageMemory;
          VkDeviceSize _imageSize;
          VkImageView _imageView;

          g_ResourceManager.CreateTexture(texturePath, &_imageMemory, &_image, &_imageSize);
          VkUtils::CreateImageView(device, _image, &_imageView, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

          g_BatchManager.m_diffuseImageList.push_back(_image);
          g_BatchManager.m_diffuseImageViewList.push_back(_imageView);
          g_BatchManager.m_diffuseImageListMemory.push_back(_imageMemory);
          g_BatchManager.m_diffuseImageListSize.push_back(_imageSize);
        }
      }
    }
  }

  std::cout << "mesh count: " << g_BatchManager.m_meshIDList.size() << std::endl;
  return true;
}
