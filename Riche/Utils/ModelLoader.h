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

// future 결과 수집 후, 기존 로직(Entt entity 생성, MiniBatch 등록 등)
static uint64_t totalVertexOffset = 0;
static uint64_t totalIndexOffset = 0;
static int totalDiffuseOffset = 0;

static bool loadObjModel(VkDevice device, const std::string& filepath, const std::string& objName, std::vector<Mesh>& outMeshes,
                         float scale = 1.0f) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  // OBJ 파일 로드
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
        // index (단순히 f를 쓰거나, unique 처리)
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

  // glTF 로드 (.gltf의 경우 ASCII, .glb의 경우 Binary 로더 사용)
  // 예시) .gltf 파일이라 가정
  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, (filepath + gltfName));
  // 만약 .glb라면:
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

  // glTF는 여러 개의 Mesh를 가질 수 있고, 각 Mesh는 여러 Primitive를 가질 수 있습니다.
  // 각각의 Primitive를 OBJ의 shape와 유사하게 취급하여 Mesh로 변환한다고 가정합니다.
  std::vector<std::future<Mesh>> futures;
  futures.reserve(model.meshes.size());  // 실제로는 mesh 수 * primitive 수만큼 생길 수 있음

  // glTF의 각 mesh -> 각 primitive 에 대하여 데이터를 추출
  for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex) {
    const tinygltf::Mesh& gltfMesh = model.meshes[meshIndex];

    for (size_t primIndex = 0; primIndex < gltfMesh.primitives.size(); ++primIndex) {
      const tinygltf::Primitive& primitive = gltfMesh.primitives[primIndex];

      // 스레드 풀에 작업을 제출
      auto future = g_ThreadPool.Submit([&, meshIndex, primIndex]() -> Mesh {
        Mesh data;

        // materialID 설정 (glTF에서는 primitive.material이 인덱스)
        // 없는 경우 -1이거나, unsigned(-1) 같은 값일 수 있으니 체크
        if (primitive.material >= 0 && primitive.material < static_cast<int>(model.materials.size())) {
          data.materialID = static_cast<int>(primitive.material);
        } else {
          data.materialID = -1;  // 기본값
        }

        // 인덱스 버퍼 처리
        std::vector<uint32_t> localIndices;
        if (primitive.indices >= 0 && primitive.indices < static_cast<int>(model.accessors.size())) {
          const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
          const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
          const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

          const uint8_t* dataPtr = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
          size_t indexCount = indexAccessor.count;
          localIndices.reserve(indexCount);

          // 인덱스 타입 (unsigned short, unsigned int 등)
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
            // 그 외 형식은 여기서 예외 처리 혹은 무시
            std::cerr << "[tinygltf] Unsupported index component type.\n";
          }
        }

        // POSITION, NORMAL, TEXCOORD_0 등 Attribute 처리
        // glTF에서 Attribute 이름: "POSITION", "NORMAL", "TEXCOORD_0" 등
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texcoords;
        std::vector<glm::vec4> colors;

        // glTF의 attributes는 std::map<std::string, int> 형태
        auto itPos = primitive.attributes.find("POSITION");
        if (itPos != primitive.attributes.end()) {
          const tinygltf::Accessor& accessor = model.accessors[itPos->second];
          const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
          const tinygltf::Buffer& buffer = model.buffers[view.buffer];

          const size_t accessorCount = accessor.count;
          positions.reserve(accessorCount);

          const uint8_t* dataPtr = buffer.data.data() + view.byteOffset + accessor.byteOffset;
          // stride 계산
          size_t stride = accessor.ByteStride(view);
          if (!stride) {
            // 기본 stride 계산
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

            // glTF에서 COLOR_0는 보통 VEC3 or VEC4
            int numComponents = tinygltf::GetNumComponentsInType(accessor.type);

            // componentType이 FLOAT / UNSIGNED_BYTE / UNSIGNED_SHORT 등이 가능
            // 여기서는 FLOAT, UNSIGNED_BYTE(정규화)만 예시
            if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT) {
              for (size_t i = 0; i < accessorCount; ++i) {
                const float* elem = reinterpret_cast<const float*>(dataPtr + stride * i);
                if (numComponents == 3) {
                  colors.push_back(glm::vec4(elem[0], elem[1], elem[2], 1.0f));
                } else {
                  // VEC4
                  colors.push_back(glm::vec4(elem[0], elem[1], elem[2], elem[3]));
                }
              }
            } else if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE) {
              // glTF는 "normalized" 플래그가 true인 경우가 많으므로
              // 0~255 범위를 0~1로 변환
              bool normalized = accessor.normalized;  // tinygltf Accessor에서 normalized 여부
              for (size_t i = 0; i < accessorCount; ++i) {
                const uint8_t* elem = reinterpret_cast<const uint8_t*>(dataPtr + stride * i);
                if (numComponents == 3) {
                  if (normalized) {
                    colors.push_back(glm::vec4(elem[0] / 255.0f, elem[1] / 255.0f, elem[2] / 255.0f, 1.0f));
                  } else {
                    // 비정규화 -> 그냥 0~255로 사용하려면, 필요에 따라 처리
                    colors.push_back(glm::vec4(elem[0], elem[1], elem[2], 1.0f));
                  }
                } else {
                  // numComponents == 4
                  if (normalized) {
                    colors.push_back(glm::vec4(elem[0] / 255.0f, elem[1] / 255.0f, elem[2] / 255.0f, elem[3] / 255.0f));
                  } else {
                    colors.push_back(glm::vec4(elem[0], elem[1], elem[2], elem[3]));
                  }
                }
              }
            } else {
              // 필요하다면 다른 componentType (e.g. UNSIGNED_SHORT) 처리
              std::cerr << "[tinygltf] Unsupported COLOR_0 component type.\n";
            }
          }
        }

        // vertex / index 최종 구성
        // tinyobjloader의 예시와 비슷하게 BasicVertex로 묶어서 data.vertices에 넣는다.
        // 인덱스가 존재하는 경우 localIndices를 통해 삼각형(또는 그 외 모드)에 맞춰 할당
        // 인덱스가 없으면 positions.size()만큼 직접 push_back 할 수 있음.
        if (!localIndices.empty()) {
          data.indices.reserve(localIndices.size());
          data.vertices.reserve(positions.size());  // rough

          // 우선 정점 정보를 모두 BasicVertex 형태로 생성
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

          // 인덱스 할당
          for (auto idx : localIndices) {
            data.indices.push_back(idx);
          }
        } else {
          // 인덱스가 없을 때. (drawArrays 방식)
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

    // 리소스 매니저를 통해 실제 버퍼 생성(업로드) 등
    g_BatchManager.AddDataToMiniBatch(g_BatchManager.m_miniBatchList, g_ResourceManager, partial);

    // 엔티티 생성 및 등록
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

    AABB _aabb = ComputeAABB(partial.vertices);  // 사용자 정의 AABB 계산 함수
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

  // glTF의 materials에 대해 텍스처 등 로딩
  // glTF에서는 PBRMetallicRoughness 구조 등에서 텍스처 index를 얻어올 수 있음
  // 아래는 baseColorTexture만 로딩하는 예시
  for (size_t i = 0; i < model.materials.size(); ++i) {
    const tinygltf::Material& mat = model.materials[i];

    // baseColorTexture에 대한 인덱스 확인
    if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
      int texIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
      if (texIndex < (int)model.textures.size()) {
        const tinygltf::Texture& texture = model.textures[texIndex];
        if (texture.source >= 0 && texture.source < (int)model.images.size()) {
          const tinygltf::Image& image = model.images[texture.source];

          std::string texturePath = image.uri;
          // 외부 파일일 경우, filepath + texturePath 등이 실제 파일 경로가 될 수도 있음
          // 경로에 '\\' 등이 들어 있을 수 있으므로 교체
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

  // glTF 로드 (.gltf의 경우 ASCII, .glb의 경우 Binary 로더 사용)
  // 여기서는 .gltf 파일이라 가정
  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, (filepath + gltfName));
  // 만약 .glb라면:
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

  // glTF는 여러 개의 Mesh를 가질 수 있고, 각 Mesh는 여러 Primitive를 가질 수 있습니다.
  // 각 Mesh의 각 Primitive를 순차적으로 처리하여 Mesh 데이터를 생성합니다.
  for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex) {
    const tinygltf::Mesh& gltfMesh = model.meshes[meshIndex];

    for (size_t primIndex = 0; primIndex < gltfMesh.primitives.size(); ++primIndex) {
      const tinygltf::Primitive& primitive = gltfMesh.primitives[primIndex];

      Mesh data;

      // materialID 설정 (glTF에서는 primitive.material이 인덱스)
      // 없는 경우 -1 혹은 unsigned(-1) 같은 값일 수 있으니 체크
      if (primitive.material >= 0 && primitive.material < static_cast<int>(model.materials.size())) {
        data.materialID = static_cast<int>(primitive.material);
      } else {
        data.materialID = -1;  // 기본값
      }

      // 인덱스 버퍼 처리
      std::vector<uint32_t> localIndices;
      if (primitive.indices >= 0 && primitive.indices < static_cast<int>(model.accessors.size())) {
        const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
        const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

        const uint8_t* dataPtr = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
        size_t indexCount = indexAccessor.count;
        localIndices.reserve(indexCount);

        // 인덱스 타입 (unsigned short, unsigned int 등)
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
        // 인덱스가 없는 경우, 예를 들어 TRIANGLES 모드라면 positions.size() 만큼 인덱스를 직접 생성
      }

      // POSITION, NORMAL, TEXCOORD_0 등 Attribute 처리
      std::vector<glm::vec3> positions;
      std::vector<glm::vec3> normals;
      std::vector<glm::vec2> texcoords;

      // POSITION 처리
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

      // NORMAL 처리
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

      // TEXCOORD_0 처리
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

      // 최종 vertex / index 구성
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
        // 인덱스가 없는 경우, drawArrays 방식으로 처리 (positions.size() 만큼 직접 인덱스 할당)
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

      // 각 primitive에서 생성한 Mesh 데이터를 바로 처리
      // (필요하면, 각 Mesh별로 엔티티 생성, 미니배치 등록 등을 수행)
      data.vertexCount = static_cast<uint32_t>(data.vertices.size());
      data.indexCount = static_cast<uint32_t>(data.indices.size());

      // 예: 미니배치에 데이터 추가
      AddDataToMiniBatch(g_BatchManager.m_miniBatchList, g_ResourceManager, data);

      // 엔티티 생성 및 등록 (예시)
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

      AABB _aabb = ComputeAABB(data.vertices);  // 사용자 정의 AABB 계산 함수
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

      // outMeshes에 생성한 Mesh 데이터 추가
      outMeshes.push_back(std::move(data));
    }
  }

  // glTF의 materials에 대해 텍스처 등 로딩 (baseColorTexture만 로딩하는 예제)
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
