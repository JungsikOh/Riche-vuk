#pragma once

#include "Utils/Singleton.h"

class RenderSetting : public Singleton<RenderSetting> {
  friend class Singleton<RenderSetting>;

 public:
  bool isWireRendering = false;
  bool isOcclusionCulling = false;
  bool isRenderBoundingBox = false;
  bool isMultiThreading = false;
  bool UseMeshShader = false;
  bool UseBasicPass = false;
  bool ForceGpuBatchRendering = true;

  int beforeCullingRenderingNum = 0;
  int afterViewCullingRenderingNum = 0;
  int afterOcclusionCullingRenderingNum = 0;

  bool changeFlag = false;
};

#define g_RenderSetting RenderSetting::Get()
