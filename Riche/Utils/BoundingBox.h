#pragma once

struct AABB
{
	glm::vec3 min;	// minimum coord
	glm::vec3 max;	// maximum coord
};

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

    return { min, max };
}