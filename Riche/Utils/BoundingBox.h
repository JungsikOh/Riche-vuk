#pragma once

struct FrustumPlane {
  glm::vec3 normal;
  float distance;
};

struct AABB {
  glm::vec4 min;  // minimum coord
  glm::vec4 max;  // maximum coord
};

struct AABBBufferList {
  VkBuffer vertexBuffer;
  VkDeviceMemory vertexBufferMemory;

  VkBuffer indexBuffer;
  VkDeviceMemory indexBufferMemory;
};

struct BoundingSphere {
  glm::vec3 center;
  float radius;
};

static std::array<FrustumPlane, 6> CalculateFrustumPlanes(const glm::mat4& viewProjectionMatrix) {
  std::array<FrustumPlane, 6> planes;

  // Right plane
  planes[0].normal =
      glm::vec3(viewProjectionMatrix[0][3] - viewProjectionMatrix[0][0], viewProjectionMatrix[1][3] - viewProjectionMatrix[1][0],
                viewProjectionMatrix[2][3] - viewProjectionMatrix[2][0]);
  planes[0].distance = viewProjectionMatrix[3][3] - viewProjectionMatrix[3][0];

  // Left plane
  planes[1].normal =
      glm::vec3(viewProjectionMatrix[0][3] + viewProjectionMatrix[0][0], viewProjectionMatrix[1][3] + viewProjectionMatrix[1][0],
                viewProjectionMatrix[2][3] + viewProjectionMatrix[2][0]);
  planes[1].distance = viewProjectionMatrix[3][3] + viewProjectionMatrix[3][0];

  // Top plane
  planes[2].normal =
      glm::vec3(viewProjectionMatrix[0][3] - viewProjectionMatrix[0][1], viewProjectionMatrix[1][3] - viewProjectionMatrix[1][1],
                viewProjectionMatrix[2][3] - viewProjectionMatrix[2][1]);
  planes[2].distance = viewProjectionMatrix[3][3] - viewProjectionMatrix[3][1];

  // Bottom plane
  planes[3].normal =
      glm::vec3(viewProjectionMatrix[0][3] + viewProjectionMatrix[0][1], viewProjectionMatrix[1][3] + viewProjectionMatrix[1][1],
                viewProjectionMatrix[2][3] + viewProjectionMatrix[2][1]);
  planes[3].distance = viewProjectionMatrix[3][3] + viewProjectionMatrix[3][1];

  // Near plane (Vulkan의 z = 0)
  planes[4].normal =
      glm::vec3(viewProjectionMatrix[0][3] + viewProjectionMatrix[0][2], viewProjectionMatrix[1][3] + viewProjectionMatrix[1][2],
                viewProjectionMatrix[2][3] + viewProjectionMatrix[2][2]);
  planes[4].distance = viewProjectionMatrix[3][3] + viewProjectionMatrix[3][2];

  // Far plane (Vulkan의 z = 1)
  planes[5].normal =
      glm::vec3(viewProjectionMatrix[0][3] - viewProjectionMatrix[0][2], viewProjectionMatrix[1][3] - viewProjectionMatrix[1][2],
                viewProjectionMatrix[2][3] - viewProjectionMatrix[2][2]);
  planes[5].distance = viewProjectionMatrix[3][3] - viewProjectionMatrix[3][2];

  // Normalize the planes (distance는 노멀의 길이로 나누지 않음)
  for (auto& plane : planes) {
    float length = glm::length(plane.normal);
    plane.normal /= length;
    plane.distance /= length;
  }

  return planes;
}

template <typename VERTEX>
static AABB ComputeAABB(const std::vector<VERTEX>& vertices) {
  glm::vec3 min(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
  glm::vec3 max(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());

  // Update min/max, visiting each point
  for (const auto& vertex : vertices) {
    min = glm::min(min, vertex.pos);
    max = glm::max(max, vertex.pos);
  }

  return {glm::vec4(min, 1.0f), glm::vec4(max, 1.0f)};
}

static std::vector<glm::vec3> CreateAABBVertexBuffer(const AABB& aabb) {
  // AABB 구조체의 min과 max는 glm::vec4 형식이지만, 여기서는 위치 정보로 사용합니다.
  glm::vec3 min = glm::vec3(aabb.min);
  glm::vec3 max = glm::vec3(aabb.max);

  // 8개의 코너를 정의합니다.
  std::vector<glm::vec3> vertices = {
      glm::vec3(min.x, min.y, min.z),  // 0
      glm::vec3(max.x, min.y, min.z),  // 1
      glm::vec3(max.x, max.y, min.z),  // 2
      glm::vec3(min.x, max.y, min.z),  // 3
      glm::vec3(min.x, min.y, max.z),  // 4
      glm::vec3(max.x, min.y, max.z),  // 5
      glm::vec3(max.x, max.y, max.z),  // 6
      glm::vec3(min.x, max.y, max.z)   // 7
  };

  return vertices;
}

static std::vector<uint32_t> CreateAABBIndexBuffer() {
  const uint32_t PRIMITIVE_RESTART = 0xFFFFFFFF;

  // 정점 배열은 아래와 같이 구성되어 있다고 가정:
  // 0: bottom front left
  // 1: bottom front right
  // 2: top front right
  // 3: top front left
  // 4: bottom back left
  // 5: bottom back right
  // 6: top back right
  // 7: top back left
  //
  // 12개 에지(라인)는 다음과 같습니다.
  // Bottom face (y = min): edges: 0-1, 1-5, 5-4, 4-0
  // Top face (y = max):    edges: 3-2, 2-6, 6-7, 7-3
  // Vertical edges:       edges: 0-3, 1-2, 5-6, 4-7

  std::vector<uint32_t> indices = {// 밑면 (y = min): v0, v1, v5, v4
                                   0, 1, 1, 5, 5, 4, 4, 0,
                                   // 윗면 (y = max): v3, v2, v6, v7
                                   3, 2, 2, 6, 6, 7, 7, 3,
                                   // 수직선 (측면)
                                   0, 3, 1, 2, 5, 6, 4, 7};


  return indices;
}

static AABB TransformAABB(const AABB& aabb, const glm::mat4& modelMatrix) {
  // AABB의 8개의 꼭짓점 계산
  glm::vec3 corners[8] = {glm::vec3(aabb.min.x, aabb.min.y, aabb.min.z), glm::vec3(aabb.min.x, aabb.min.y, aabb.max.z),
                          glm::vec3(aabb.min.x, aabb.max.y, aabb.min.z), glm::vec3(aabb.min.x, aabb.max.y, aabb.max.z),
                          glm::vec3(aabb.max.x, aabb.min.y, aabb.min.z), glm::vec3(aabb.max.x, aabb.min.y, aabb.max.z),
                          glm::vec3(aabb.max.x, aabb.max.y, aabb.min.z), glm::vec3(aabb.max.x, aabb.max.y, aabb.max.z)};

  // 변환된 꼭짓점들을 사용하여 새로운 AABB 계산
  glm::vec3 newMin(std::numeric_limits<float>::max());
  glm::vec3 newMax(std::numeric_limits<float>::lowest());

  for (int i = 0; i < 8; ++i) {
    glm::vec4 transformedCorner = modelMatrix * glm::vec4(corners[i], 1.0f);
    newMin = glm::min(newMin, glm::vec3(transformedCorner));
    newMax = glm::max(newMax, glm::vec3(transformedCorner));
  }

  return {glm::vec4(newMin, 1.0f), glm::vec4(newMax, 1.0f)};
}

static BoundingSphere ComputeBoundingSphere(const std::vector<glm::vec3>& vertices) {
  // 1. 모든 정점의 중심을 계산합니다.
  glm::vec3 center(0.0f);
  for (const auto& vertex : vertices) {
    center += vertex;
  }
  center /= static_cast<float>(vertices.size());

  // 2. 중심점으로부터 각 정점까지의 거리 중 가장 큰 값을 반지름으로 설정합니다.
  float radius = 0.0f;
  for (const auto& vertex : vertices) {
    float distance = glm::length(vertex - center);
    radius = glm::max(radius, distance);
  }

  return {center, radius};
}