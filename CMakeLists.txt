cmake_minimum_required(VERSION 3.24)

project(RicheVulkan VERSION 0.1 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "Riche currently targets Windows and Visual Studio.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

set(RICHE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Riche")
set(THIRD_PARTY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty")

set(RICHE_VULKAN_SDK "C:/VulkanSDK/1.3.290.0" CACHE PATH "Fallback Vulkan SDK path")
if(NOT DEFINED ENV{VULKAN_SDK} AND EXISTS "${RICHE_VULKAN_SDK}/Include/vulkan/vulkan.h")
  set(ENV{VULKAN_SDK} "${RICHE_VULKAN_SDK}")
endif()
if(NOT Vulkan_INCLUDE_DIR AND EXISTS "${RICHE_VULKAN_SDK}/Include/vulkan/vulkan.h")
  set(Vulkan_INCLUDE_DIR "${RICHE_VULKAN_SDK}/Include" CACHE PATH "Vulkan include directory")
endif()
if(NOT Vulkan_LIBRARY AND EXISTS "${RICHE_VULKAN_SDK}/Lib/vulkan-1.lib")
  set(Vulkan_LIBRARY "${RICHE_VULKAN_SDK}/Lib/vulkan-1.lib" CACHE FILEPATH "Vulkan loader library")
endif()
find_package(Vulkan REQUIRED)

set(MESHOPTIMIZER_ROOT "${THIRD_PARTY_ROOT}/meshoptimizer")
if(NOT EXISTS "${MESHOPTIMIZER_ROOT}/src/meshoptimizer.h")
  if(EXISTS "${THIRD_PARTY_ROOT}/meshoptimizer.zip")
    message(STATUS "Extracting bundled meshoptimizer dependency")
    file(MAKE_DIRECTORY "${MESHOPTIMIZER_ROOT}")
    file(ARCHIVE_EXTRACT INPUT "${THIRD_PARTY_ROOT}/meshoptimizer.zip" DESTINATION "${MESHOPTIMIZER_ROOT}")
  else()
    message(FATAL_ERROR "Missing meshoptimizer source. Initialize ThirdParty/meshoptimizer or provide ThirdParty/meshoptimizer.zip.")
  endif()
endif()

set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(MESHOPT_WERROR OFF CACHE BOOL "" FORCE)
add_subdirectory("${MESHOPTIMIZER_ROOT}" "${CMAKE_BINARY_DIR}/ThirdParty/meshoptimizer")
set_target_properties(meshoptimizer PROPERTIES FOLDER "ThirdParty")

add_library(tinyobjloader STATIC "${THIRD_PARTY_ROOT}/tinyobjloader/tiny_obj_loader.cc")
target_include_directories(tinyobjloader PUBLIC "${THIRD_PARTY_ROOT}/tinyobjloader")
set_target_properties(tinyobjloader PROPERTIES FOLDER "ThirdParty")

add_library(tinygltf STATIC "${THIRD_PARTY_ROOT}/tinygltf/tiny_gltf.cc")
target_include_directories(tinygltf PUBLIC "${THIRD_PARTY_ROOT}/tinygltf")
set_target_properties(tinygltf PROPERTIES FOLDER "ThirdParty")

add_library(glfw3::glfw3 SHARED IMPORTED GLOBAL)
set_target_properties(glfw3::glfw3 PROPERTIES
  IMPORTED_IMPLIB "${THIRD_PARTY_ROOT}/GLFW/lib-vc2022/glfw3dll.lib"
  IMPORTED_LOCATION "${THIRD_PARTY_ROOT}/GLFW/lib-vc2022/glfw3.dll"
  INTERFACE_INCLUDE_DIRECTORIES "${THIRD_PARTY_ROOT}/GLFW/include"
)

add_library(imgui STATIC
  "${THIRD_PARTY_ROOT}/imgui/imgui.cpp"
  "${THIRD_PARTY_ROOT}/imgui/ImGuizmo.cpp"
  "${THIRD_PARTY_ROOT}/imgui/imgui_demo.cpp"
  "${THIRD_PARTY_ROOT}/imgui/imgui_draw.cpp"
  "${THIRD_PARTY_ROOT}/imgui/imgui_impl_glfw.cpp"
  "${THIRD_PARTY_ROOT}/imgui/imgui_impl_vulkan.cpp"
  "${THIRD_PARTY_ROOT}/imgui/imgui_tables.cpp"
  "${THIRD_PARTY_ROOT}/imgui/imgui_widgets.cpp"
)
target_include_directories(imgui PUBLIC
  "${THIRD_PARTY_ROOT}/imgui"
  "${THIRD_PARTY_ROOT}/GLFW/include"
)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan glfw3::glfw3)
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
set_target_properties(imgui PROPERTIES FOLDER "ThirdParty")

add_library(ImGuiFileDialog STATIC "${THIRD_PARTY_ROOT}/ImGuiFileDialog/ImGuiFileDialog.cpp")
target_include_directories(ImGuiFileDialog PUBLIC
  "${THIRD_PARTY_ROOT}/ImGuiFileDialog"
  "${THIRD_PARTY_ROOT}/ImGuiFileDialog/dirent"
  "${THIRD_PARTY_ROOT}/ImGuiFileDialog/stb"
  "${THIRD_PARTY_ROOT}/imgui"
)
target_link_libraries(ImGuiFileDialog PUBLIC imgui)
set_target_properties(ImGuiFileDialog PROPERTIES FOLDER "ThirdParty")

file(GLOB_RECURSE RICHE_SOURCES CONFIGURE_DEPENDS
  "${RICHE_ROOT}/*.cpp"
)
list(FILTER RICHE_SOURCES EXCLUDE REGEX ".*/extern/.*")

file(GLOB_RECURSE RICHE_HEADERS CONFIGURE_DEPENDS
  "${RICHE_ROOT}/*.h"
  "${RICHE_ROOT}/*.hpp"
)
list(FILTER RICHE_HEADERS EXCLUDE REGEX ".*/extern/.*")

file(GLOB_RECURSE RICHE_SHADERS CONFIGURE_DEPENDS
  "${RICHE_ROOT}/Resources/Shaders/*.glsl"
  "${RICHE_ROOT}/Resources/Shaders/*.vert"
  "${RICHE_ROOT}/Resources/Shaders/*.frag"
  "${RICHE_ROOT}/Resources/Shaders/*.comp"
  "${RICHE_ROOT}/Resources/Shaders/*.mesh"
  "${RICHE_ROOT}/Resources/Shaders/*.task"
  "${RICHE_ROOT}/Resources/Shaders/*.rgen"
  "${RICHE_ROOT}/Resources/Shaders/*.rchit"
  "${RICHE_ROOT}/Resources/Shaders/*.rmiss"
)
set_source_files_properties(${RICHE_SHADERS} PROPERTIES HEADER_FILE_ONLY ON)

add_executable(Riche ${RICHE_SOURCES} ${RICHE_HEADERS} ${RICHE_SHADERS})
target_precompile_headers(Riche PRIVATE "${RICHE_ROOT}/pch.h")

target_include_directories(Riche PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${RICHE_ROOT}"
  "${THIRD_PARTY_ROOT}"
  "${THIRD_PARTY_ROOT}/GLFW/include"
  "${THIRD_PARTY_ROOT}/GLM"
  "${THIRD_PARTY_ROOT}/imgui"
  "${THIRD_PARTY_ROOT}/ImGuiFileDialog"
  "${THIRD_PARTY_ROOT}/tinyobjloader"
)

target_compile_definitions(Riche PRIVATE _CRT_SECURE_NO_WARNINGS _WINDOWS)
if(MSVC)
  target_compile_options(Riche PRIVATE /W3 /sdl /permissive- /utf-8)
endif()

target_link_libraries(Riche PRIVATE
  Vulkan::Vulkan
  glfw3::glfw3
  imgui
  ImGuiFileDialog
  tinyobjloader
  tinygltf
  meshoptimizer
)

set_target_properties(Riche PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${RICHE_ROOT}"
)

add_custom_command(TARGET Riche POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${THIRD_PARTY_ROOT}/GLFW/lib-vc2022/glfw3.dll"
          "$<TARGET_FILE_DIR:Riche>"
  VERBATIM
)

source_group(TREE "${RICHE_ROOT}" PREFIX "Riche" FILES ${RICHE_SOURCES} ${RICHE_HEADERS} ${RICHE_SHADERS})

find_program(GLSLANG_VALIDATOR
  NAMES glslangValidator
  HINTS "$ENV{VULKAN_SDK}/Bin" "C:/VulkanSDK/1.3.290.0/Bin"
)

if(GLSLANG_VALIDATOR)
  set(SHADER_ROOT "${RICHE_ROOT}/Resources/Shaders")
  set(RICHE_SHADER_OUTPUTS)

  function(riche_add_shader SOURCE OUTPUT)
    add_custom_command(
      OUTPUT "${SHADER_ROOT}/${OUTPUT}"
      COMMAND "${GLSLANG_VALIDATOR}" ${ARGN} -o "${OUTPUT}" -V "${SOURCE}"
      DEPENDS "${SHADER_ROOT}/${SOURCE}"
      WORKING_DIRECTORY "${SHADER_ROOT}"
      VERBATIM
    )
    list(APPEND RICHE_SHADER_OUTPUTS "${SHADER_ROOT}/${OUTPUT}")
    set(RICHE_SHADER_OUTPUTS "${RICHE_SHADER_OUTPUTS}" PARENT_SCOPE)
  endfunction()

  riche_add_shader("LightingVS.vert" "LightingVS.spv")
  riche_add_shader("LightingPS.frag" "LightingPS.spv")
  riche_add_shader("BasicLightingVS.vert" "BasicLightingVS.spv")
  riche_add_shader("BasicLightingPS.frag" "BasicLightingPS.spv")
  riche_add_shader("ObjectIdVS.vert" "ObjectIdVS.spv")
  riche_add_shader("ObjectIdPS.frag" "ObjectIdPS.spv")
  riche_add_shader("DepthOnlyVS.vert" "DepthOnlyVS.spv")
  riche_add_shader("BoundingBoxVS.vert" "BoundingBoxVS.spv")
  riche_add_shader("BoundingBoxPS.frag" "BoundingBoxPS.spv")
  riche_add_shader("ViewFrustumCullingCS.comp" "ViewFrustumCullingCS.spv")
  riche_add_shader("HiZOcclusionCullingCS.comp" "HiZOcclusionCullingCS.spv")
  riche_add_shader("RenderingQuadVS.vert" "RenderingQuadVS.spv")
  riche_add_shader("RenderingQuadPS.frag" "RenderingQuadPS.spv")
  riche_add_shader("BasicTS.task" "BasicTS.spv" --target-env vulkan1.3)
  riche_add_shader("BasicMS.mesh" "BasicMS.spv" --target-env vulkan1.3)
  riche_add_shader("BasicPS.frag" "BasicPS.spv" --target-env vulkan1.3)
  riche_add_shader("RaytracingShadow/Raygen.rgen" "RaytracingShadow/Raygen.rgen.spv" --target-env vulkan1.3)
  riche_add_shader("RaytracingShadow/ClosestHit.rchit" "RaytracingShadow/ClosestHit.rchit.spv" --target-env vulkan1.3)
  riche_add_shader("RaytracingShadow/Miss.rmiss" "RaytracingShadow/Miss.rmiss.spv" --target-env vulkan1.3)
  riche_add_shader("RaytracingShadow/Shadow.rmiss" "RaytracingShadow/Shadow.rmiss.spv" --target-env vulkan1.3)

  add_custom_target(RicheShaders DEPENDS ${RICHE_SHADER_OUTPUTS})
  set_target_properties(RicheShaders PROPERTIES FOLDER "Shaders")
else()
  message(WARNING "glslangValidator was not found. The optional RicheShaders target will not be generated.")
endif()
