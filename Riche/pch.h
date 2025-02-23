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

//
// 3rd party
//

// pass the certain waring messeage
#pragma warning(push)
#pragma warning(disable : 4819)

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <stb_image.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "ImGuizmo.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"  // if use GLFW.

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <entt/entt.hpp>

#include <tiny_obj_loader.h>
#include <tinygltf/tiny_gltf.h>

#pragma warning(pop)


#include "Rendering/Core.h"