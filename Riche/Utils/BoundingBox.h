#pragma once

struct FrustumPlane {
    glm::vec3 normal;
    float distance;
};

struct AABB
{
    glm::vec4 min;	// minimum coord
    glm::vec4 max;	// maximum coord
};

// 6개의 평면을 구하는 함수
static std::array<FrustumPlane, 6> CalculateFrustumPlanes(const glm::mat4& viewProjectionMatrix) {
    std::array<FrustumPlane, 6> planes;

    // Right plane
    planes[0].normal = glm::vec3(viewProjectionMatrix[0][3] - viewProjectionMatrix[0][0],
        viewProjectionMatrix[1][3] - viewProjectionMatrix[1][0],
        viewProjectionMatrix[2][3] - viewProjectionMatrix[2][0]);
    planes[0].distance = viewProjectionMatrix[3][3] - viewProjectionMatrix[3][0];

    // Left plane
    planes[1].normal = glm::vec3(viewProjectionMatrix[0][3] + viewProjectionMatrix[0][0],
        viewProjectionMatrix[1][3] + viewProjectionMatrix[1][0],
        viewProjectionMatrix[2][3] + viewProjectionMatrix[2][0]);
    planes[1].distance = viewProjectionMatrix[3][3] + viewProjectionMatrix[3][0];

    // Top plane
    planes[2].normal = glm::vec3(viewProjectionMatrix[0][3] - viewProjectionMatrix[0][1],
        viewProjectionMatrix[1][3] - viewProjectionMatrix[1][1],
        viewProjectionMatrix[2][3] - viewProjectionMatrix[2][1]);
    planes[2].distance = viewProjectionMatrix[3][3] - viewProjectionMatrix[3][1];

    // Bottom plane
    planes[3].normal = glm::vec3(viewProjectionMatrix[0][3] + viewProjectionMatrix[0][1],
        viewProjectionMatrix[1][3] + viewProjectionMatrix[1][1],
        viewProjectionMatrix[2][3] + viewProjectionMatrix[2][1]);
    planes[3].distance = viewProjectionMatrix[3][3] + viewProjectionMatrix[3][1];

    // Near plane
    planes[4].normal = glm::vec3(viewProjectionMatrix[0][3] + viewProjectionMatrix[0][2],
        viewProjectionMatrix[1][3] + viewProjectionMatrix[1][2],
        viewProjectionMatrix[2][3] + viewProjectionMatrix[2][2]);
    planes[4].distance = viewProjectionMatrix[3][3] + viewProjectionMatrix[3][2];

    // Far plane
    planes[5].normal = glm::vec3(viewProjectionMatrix[0][3] - viewProjectionMatrix[0][2],
        viewProjectionMatrix[1][3] - viewProjectionMatrix[1][2],
        viewProjectionMatrix[2][3] - viewProjectionMatrix[2][2]);
    planes[5].distance = viewProjectionMatrix[3][3] - viewProjectionMatrix[3][2];

    // Normalize the planes
    for (auto& plane : planes) {
        float length = glm::length(plane.normal);
        plane.normal /= length;
        plane.distance /= length;
    }

    return planes;
}

static AABB ComputeAABB(const std::vector<glm::vec3>& vertices)
{
    glm::vec3 min(
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    );
    glm::vec3 max(
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    );

    // Update min/max, visiting each point
    for (const auto& point : vertices) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    return { glm::vec4(min, 1.0f), glm::vec4(max, 1.0f) };
}