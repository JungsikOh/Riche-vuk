#pragma once

#include "BoundingBox.h"
#include "Rendering/Components.h"
#include "Rendering/Image.h"
#include "Rendering/Mesh.h"
#include "Rendering/VulkanRenderer.h"
#include "Singleton.h"
#include "ThreadPool.h"
#include "VkUtils/ResourceManager.h"

static entt::registry g_Registry;

static uint64_t totalVertexOffset = 0;
static uint64_t totalIndexOffset = 0;
static int totalDiffuseOffset = 0;

static bool loadObjModel(VkDevice device, const std::string& filepath, const std::string& objName, std::vector<Mesh>& outMeshes,
                         float scale = 1.0f) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

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
        data.indices.push_back(static_cast<uint32_t>(f));
      }
      return data;
    });

    futures.push_back(std::move(future));
  }

  size_t initObjectIDSize = g_BatchManager.m_objectIDList.size();
  for (int i = 0; i < futures.size(); ++i) {
    auto& f = futures[i];

    Mesh partial = f.get();
    partial.vertexCount = static_cast<uint32_t>(partial.vertices.size());
    partial.indexCount = static_cast<uint32_t>(partial.indices.size());

    AddDataToMiniBatch(g_BatchManager.m_miniBatchList, g_ResourceManager, partial);

    entt::entity object = g_Registry.create();
    ObjectID _id;
    _id.materialID = partial.materialID;
    g_BatchManager.m_objectIDList.push_back(_id);
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

      GpuImage _image;

      g_ResourceManager.CreateTexture(texturePath, &_image.memory, &_image.image, &_image.size);
      VkUtils::CreateImageView(device, _image.image, &_image.imageView, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

      g_BatchManager.m_diffuseImages.push_back(_image);
    }
  }

  return true;
}

static bool loadGltfModel(VkDevice device, const std::string& filepath, const std::string& gltfName, std::vector<Mesh>& outMeshes,
                          float scale = 1.0f, glm::vec3 pos = glm::vec3(0.0f)) {
  tinygltf::TinyGLTF loader;
  tinygltf::Model model;
  std::string warn, err;

  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, (filepath + gltfName));
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

  std::vector<std::future<Mesh>> futures;
  futures.reserve(model.meshes.size());

  for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex) {
    const tinygltf::Mesh& gltfMesh = model.meshes[meshIndex];

    for (size_t primIndex = 0; primIndex < gltfMesh.primitives.size(); ++primIndex) {
      const tinygltf::Primitive& primitive = gltfMesh.primitives[primIndex];

      auto future = g_ThreadPool.Submit([&, meshIndex, primIndex]() -> Mesh {
        Mesh data;

        if (primitive.material >= 0 && primitive.material < static_cast<int>(model.materials.size())) {
          data.materialID = static_cast<int>(primitive.material);
        } else {
          data.materialID = -1;
        }

        std::vector<uint32_t> localIndices;
        if (primitive.indices >= 0 && primitive.indices < static_cast<int>(model.accessors.size())) {
          const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
          const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
          const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

          const uint8_t* dataPtr = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
          size_t indexCount = indexAccessor.count;
          localIndices.reserve(indexCount);

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
        }

        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texcoords;
        std::vector<glm::vec4> colors;

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

        {
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
        }

        {
          auto itColor = primitive.attributes.find("COLOR_0");
          if (itColor != primitive.attributes.end()) {
            const tinygltf::Accessor& accessor = model.accessors[itColor->second];
            const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[view.buffer];

            size_t accessorCount = accessor.count;
            colors.reserve(accessorCount);

            const uint8_t* dataPtr = buffer.data.data() + view.byteOffset + accessor.byteOffset;
            size_t stride = accessor.ByteStride(view);
            if (!stride) {
              stride = tinygltf::GetNumComponentsInType(accessor.type) * tinygltf::GetComponentSizeInBytes(accessor.componentType);
            }

            int numComponents = tinygltf::GetNumComponentsInType(accessor.type);

            if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT) {
              for (size_t i = 0; i < accessorCount; ++i) {
                const float* elem = reinterpret_cast<const float*>(dataPtr + stride * i);
                if (numComponents == 3) {
                  colors.push_back(glm::vec4(elem[0], elem[1], elem[2], 1.0f));
                } else {
                  colors.push_back(glm::vec4(elem[0], elem[1], elem[2], elem[3]));
                }
              }
            } else if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE) {
              bool normalized = accessor.normalized;
              for (size_t i = 0; i < accessorCount; ++i) {
                const uint8_t* elem = reinterpret_cast<const uint8_t*>(dataPtr + stride * i);
                if (numComponents == 3) {
                  if (normalized) {
                    colors.push_back(glm::vec4(elem[0] / 255.0f, elem[1] / 255.0f, elem[2] / 255.0f, 1.0f));
                  } else {
                    colors.push_back(glm::vec4(elem[0], elem[1], elem[2], 1.0f));
                  }
                } else {
                  if (normalized) {
                    colors.push_back(glm::vec4(elem[0] / 255.0f, elem[1] / 255.0f, elem[2] / 255.0f, elem[3] / 255.0f));
                  } else {
                    colors.push_back(glm::vec4(elem[0], elem[1], elem[2], elem[3]));
                  }
                }
              }
            } else {
              std::cerr << "[tinygltf] Unsupported COLOR_0 component type.\n";
            }
          }
        }

        if (!localIndices.empty()) {
          data.indices.reserve(localIndices.size());
          data.vertices.reserve(positions.size());

          for (size_t i = 0; i < positions.size(); ++i) {
            BasicVertex v{};
            v.pos = positions[i];
            v.normal = (i < normals.size()) ? normals[i] : glm::vec3(0, 0, 0);
            v.tex = (i < texcoords.size()) ? texcoords[i] : glm::vec2(0, 0);
            data.vertices.push_back(v);

            RayTracingVertex rv{};
            rv.pos = glm::vec4(v.pos, 1.0f);
            rv.normal = glm::vec4(v.normal, 1.0f);
            rv.tex = v.tex;
            rv.padd[0] = meshIndex;
            data.ray_vertices.push_back(rv);
          }

          for (auto idx : localIndices) {
            data.indices.push_back(idx);
          }
        } else {
          data.vertices.reserve(positions.size());

          for (size_t i = 0; i < positions.size(); ++i) {
            BasicVertex v{};
            v.pos = positions[i];
            v.normal = (i < normals.size()) ? normals[i] : glm::vec3(0, 0, 0);
            v.tex = (i < texcoords.size()) ? texcoords[i] : glm::vec2(0, 0);
            data.vertices.push_back(v);

            RayTracingVertex rv{};
            rv.pos = glm::vec4(v.pos, 1.0f);
            rv.normal = glm::vec4(v.normal, 1.0f);
            rv.tex = v.tex;
            rv.padd[0] = meshIndex;
            data.ray_vertices.push_back(rv);

            data.indices.push_back(static_cast<uint32_t>(i));
          }
        }

        return data;
      });

      futures.push_back(std::move(future));
    }
  }

  totalDiffuseOffset = g_BatchManager.m_diffuseImages.size();

  for (auto& f : futures) {
    Mesh partial = f.get();

    partial.vertexCount = static_cast<uint32_t>(partial.ray_vertices.size());
    partial.indexCount = static_cast<uint32_t>(partial.indices.size());

    g_BatchManager.AddDataToMiniBatch(g_BatchManager.m_miniBatchList, g_ResourceManager, partial);

    entt::entity object = g_Registry.create();

    ObjectID _id;
    _id.materialID = partial.materialID + totalDiffuseOffset;
    g_BatchManager.m_objectIDList.push_back(_id);
    g_Registry.emplace<ObjectID>(object, _id);

    Transform _transform = {};
    _transform.startTransform = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
    _transform.currentTransform = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
    _transform.currentTransform = glm::translate(_transform.currentTransform, pos);

    g_BatchManager.m_trasformList.push_back(_transform);
    g_Registry.emplace<Transform>(object, _transform);

    AABB _aabb = ComputeAABB(partial.vertices);
    g_BatchManager.m_boundingBoxList.push_back(_aabb);
    g_Registry.emplace<AABB>(object, _aabb);

    std::vector<glm::vec3> AABBvertex = CreateAABBVertexBuffer(_aabb);
    std::vector<uint32_t> AABBIndics = CreateAABBIndexBuffer();

    AABBBufferList _aabbBufferList;
    g_ResourceManager.CreateVertexBuffer(AABBvertex.size() * sizeof(glm::vec3), &_aabbBufferList.vertexBufferMemory,
                                         &_aabbBufferList.vertexBuffer, AABBvertex.data());
    g_ResourceManager.CreateIndexBuffer(AABBIndics.size() * sizeof(uint32_t), &_aabbBufferList.indexBufferMemory,
                                        &_aabbBufferList.indexBuffer, AABBIndics.data());
    g_BatchManager.m_boundingBoxBufferList.push_back(_aabbBufferList);

    GpuBuffer vertexBuffer;
    GpuBuffer indexBuffer;

    g_ResourceManager.CreateVertexBuffer(partial.vertices.size() * sizeof(BasicVertex), &vertexBuffer.memory, &vertexBuffer.buffer,
                                         partial.vertices.data());
    g_ResourceManager.CreateIndexBuffer(partial.indices.size() * sizeof(uint32_t), &indexBuffer.memory, &indexBuffer.buffer,
                                        partial.indices.data());

    g_BatchManager.m_bbVertexBuffers.push_back(std::move(vertexBuffer));
    g_BatchManager.m_bbIndexBuffers.push_back(std::move(indexBuffer));

    outMeshes.push_back(std::move(partial));
  }

  for (size_t i = 0; i < model.materials.size(); ++i) {
    const tinygltf::Material& mat = model.materials[i];

    if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
      int texIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
      if (texIndex < (int)model.textures.size()) {
        const tinygltf::Texture& texture = model.textures[texIndex];
        if (texture.source >= 0 && texture.source < (int)model.images.size()) {
          const tinygltf::Image& image = model.images[texture.source];

          std::string texturePath = image.uri;
          std::replace(texturePath.begin(), texturePath.end(), '\\', '/');

          if (texturePath.find(":") == std::string::npos) {
            texturePath = filepath + texturePath;
          }

          GpuImage _image;

          g_ResourceManager.CreateTexture(texturePath, &_image.memory, &_image.image, &_image.size);
          VkUtils::CreateImageView(device, _image.image, &_image.imageView, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

          g_BatchManager.m_diffuseImages.push_back(_image);
        }
      }
    }
  }

  std::cout << "mesh count: " << g_BatchManager.m_objectIDList.size() << std::endl;
  return true;
}

static bool loadSeqGltfModel(VkDevice device, const std::string& filepath, const std::string& gltfName, std::vector<Mesh>& outMeshes,
                             float scale = 1.0f) {
  tinygltf::TinyGLTF loader;
  tinygltf::Model model;
  std::string warn, err;

  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, (filepath + gltfName));
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

  for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex) {
    const tinygltf::Mesh& gltfMesh = model.meshes[meshIndex];

    for (size_t primIndex = 0; primIndex < gltfMesh.primitives.size(); ++primIndex) {
      const tinygltf::Primitive& primitive = gltfMesh.primitives[primIndex];

      Mesh data;

      if (primitive.material >= 0 && primitive.material < static_cast<int>(model.materials.size())) {
        data.materialID = static_cast<int>(primitive.material);
      } else {
        data.materialID = -1;
      }

      std::vector<uint32_t> localIndices;
      if (primitive.indices >= 0 && primitive.indices < static_cast<int>(model.accessors.size())) {
        const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
        const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

        const uint8_t* dataPtr = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
        size_t indexCount = indexAccessor.count;
        localIndices.reserve(indexCount);

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
      }

      std::vector<glm::vec3> positions;
      std::vector<glm::vec3> normals;
      std::vector<glm::vec2> texcoords;

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

      data.vertexCount = static_cast<uint32_t>(data.vertices.size());
      data.indexCount = static_cast<uint32_t>(data.indices.size());

      AddDataToMiniBatch(g_BatchManager.m_miniBatchList, g_ResourceManager, data);

      entt::entity object = g_Registry.create();
      ObjectID _id;
      _id.materialID = data.materialID;
      g_BatchManager.m_objectIDList.push_back(_id);
      g_Registry.emplace<ObjectID>(object, _id);

      Transform _transform = {};
      _transform.startTransform = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
      _transform.currentTransform = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
      g_BatchManager.m_trasformList.push_back(_transform);
      g_Registry.emplace<Transform>(object, _transform);

      AABB _aabb = ComputeAABB(data.vertices);
      g_BatchManager.m_boundingBoxList.push_back(_aabb);
      g_Registry.emplace<AABB>(object, _aabb);

      std::vector<glm::vec3> AABBvertex = CreateAABBVertexBuffer(_aabb);
      std::vector<uint32_t> AABBIndics = CreateAABBIndexBuffer();

      AABBBufferList _aabbBufferList;
      size_t temp = AABBvertex.size() * sizeof(glm::vec3);
      g_ResourceManager.CreateVertexBuffer(AABBvertex.size() * sizeof(glm::vec3), &_aabbBufferList.vertexBufferMemory,
                                           &_aabbBufferList.vertexBuffer, AABBvertex.data());
      g_ResourceManager.CreateIndexBuffer(AABBIndics.size() * sizeof(uint32_t), &_aabbBufferList.indexBufferMemory,
                                          &_aabbBufferList.indexBuffer, AABBIndics.data());
      g_BatchManager.m_boundingBoxBufferList.push_back(_aabbBufferList);

      outMeshes.push_back(std::move(data));
    }
  }

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

          GpuImage _image;

          g_ResourceManager.CreateTexture(texturePath, &_image.memory, &_image.image, &_image.size);
          VkUtils::CreateImageView(device, _image.image, &_image.imageView, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

          g_BatchManager.m_diffuseImages.push_back(_image);
        }
      }
    }
  }

  std::cout << "mesh count: " << g_BatchManager.m_objectIDList.size() << std::endl;
  return true;
}
