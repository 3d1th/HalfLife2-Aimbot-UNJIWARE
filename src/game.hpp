#pragma once
#include "types.hpp"
namespace uw {
bool InitializeGame();
bool GameMatrix(Matrix4x4& matrix);
void CaptureGameInput(bool capture);
void GameHeartbeat();
}
