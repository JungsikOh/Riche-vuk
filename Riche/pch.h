#pragma once
#include <iostream>
#include <array>
#include <vector>
#include <optional>
#include <mutex>
#include <string>
#include <set>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include <fstream>
#include <assert.h>
#include <random>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"  // if use GLFW.

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

#include "entt/entt.hpp"

#include "Rendering/Core.h"