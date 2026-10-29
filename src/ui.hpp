#pragma once
#include "state.hpp"
#include <imgui.h>

namespace uw {
void InitializeUiFonts();
void DrawUi(bool& open,bool preview=false);
void DrawFov(float width,float height,const Matrix4x4* matrix=nullptr);
void DrawEsp(const Matrix4x4& matrix,float width,float height);
}
