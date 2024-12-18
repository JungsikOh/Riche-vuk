#pragma once

#include "Utils/Singleton.h"

class RenderSetting : public Singleton<RenderSetting> {
  friend class Singleton<RenderSetting>;

 public:
  bool isWireRendering = false;
};

#define g_RenderSetting RenderSetting::Get()