#pragma once

#include "Utils/BoundingBox.h"

#define COMPONENTS

struct Model 
{
    glm::mat4 model;
};

struct ViewProjection
{
    glm::mat4 view;
    glm::mat4 projection;
};

struct BasicVertex
{
    glm::vec3 pos;	// Vertex pos (x, y, z)
    glm::vec3 col;	// Vertex colour (r, g, b)
    glm::vec2 tex;	// Texture Coords (u, v)
};

struct COMPONENTS Transform
{
    glm::mat4 startTransform;
    glm::mat4 currentTransform;
};

struct COMPONENTS BoundingBox
{
    AABB originalBox;
    AABB currentBoxBox;
};