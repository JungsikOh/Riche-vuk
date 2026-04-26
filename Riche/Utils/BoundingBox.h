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

  planes[4].normal =
      glm::vec3(viewProjectionMatrix[0][3] + viewProjectionMatrix[0][2], viewProjectionMatrix[1][3] + viewProjectionMatrix[1][2],
                viewProjectionMatrix[2][3] + viewProjectionMatrix[2][2]);
  planes[4].distance = viewProjectionMatrix[3][3] + viewProjectionMatrix[3][2];

  planes[5].normal =
      glm::vec3(viewProjectionMatrix[0][3] - viewProjectionMatrix[0][2], viewProjectionMatrix[1][3] - viewProjectionMatrix[1][2],
                viewProjectionMatrix[2][3] - viewProjectionMatrix[2][2]);
  planes[5].distance = viewProjectionMatrix[3][3] - viewProjectionMatrix[3][2];

  for (auto& plane : planes) {
    float length = glm::length(plane.normal);
    plane.normal /= length;
    plane.distance /= length;
  }

  return planes;
}

static bool isAABBInsideFrustum(const std::array<FrustumPlane, 6>& frustum, const AABB& aabb) {
  for (int i = 0; i < 6; i++) {
    const FrustumPlane& plane = frustum[i];

    glm::vec3 positiveVertex =
        glm::vec3(
            (plane.normal.x < 0) ? aabb.min.x : aabb.max.x, 
            (plane.normal.y < 0) ? aabb.min.y : aabb.max.y,
            (plane.normal.z < 0) ? aabb.min.z : aabb.max.z);

    float distance = glm::dot(plane.normal, positiveVertex) + plane.distance;

    if (distance >= 0) {
      return true;
    }
  }
  return false;
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
  glm::vec3 min = glm::vec3(aabb.min);
  glm::vec3 max = glm::vec3(aabb.max);

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
  // 0: bottom front left
  // 1: bottom front right
  // 2: top front right
  // 3: top front left
  // 4: bottom back left
  // 5: bottom back right
  // 6: top back right
  // 7: top back left
  //
  // Bottom face (y = min): edges: 0-1, 1-5, 5-4, 4-0
  // Top face (y = max):    edges: 3-2, 2-6, 6-7, 7-3
  // Vertical edges:       edges: 0-3, 1-2, 5-6, 4-7

  std::vector<uint32_t> indices = {
      // Front (z = min)
      0, 1, 2, 2, 3, 0,

      // Back (z = max)
      4, 5, 6, 6, 7, 4,

      // Bottom (y = min)
      0, 1, 5, 5, 4, 0,

      // Top (y = max)
      3, 2, 6, 6, 7, 3,

      // Left (x = min)
      0, 4, 7, 7, 3, 0,

      // Right (x = max)
      1, 2, 6, 6, 5, 1};

  return indices;
}

static AABB TransformAABB(const AABB& aabb, const glm::mat4& modelMatrix) {
  glm::vec3 corners[8] = {glm::vec3(aabb.min.x, aabb.min.y, aabb.min.z), glm::vec3(aabb.min.x, aabb.min.y, aabb.max.z),
                          glm::vec3(aabb.min.x, aabb.max.y, aabb.min.z), glm::vec3(aabb.min.x, aabb.max.y, aabb.max.z),
                          glm::vec3(aabb.max.x, aabb.min.y, aabb.min.z), glm::vec3(aabb.max.x, aabb.min.y, aabb.max.z),
                          glm::vec3(aabb.max.x, aabb.max.y, aabb.min.z), glm::vec3(aabb.max.x, aabb.max.y, aabb.max.z)};

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
  glm::vec3 center(0.0f);
  for (const auto& vertex : vertices) {
    center += vertex;
  }
  center /= static_cast<float>(vertices.size());

  float radius = 0.0f;
  for (const auto& vertex : vertices) {
    float distance = glm::length(vertex - center);
    radius = glm::max(radius, distance);
  }

  return {center, radius};
}
