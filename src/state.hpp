#pragma once
#include "types.hpp"
#include <array>
#include <mutex>
#include <string>
#include <vector>

namespace uw {
struct Settings {
    bool esp=false,boxes=true,names=true,health=true,distance=true,espVisibility=true;
    bool combine=true,zombies=true,antlions=true,headcrabs=true,other=true;
    bool silentAim=false,visibleOnly=true,drawFov=true,noSpread=false,noRecoil=false;
    bool watermark=true;
    int activation=0;
    float aimFov=9.0f,maxDistance=100.0f;
    std::array<float,3> accent{0.0f,0.67f,0.92f};
};
struct NpcRecord {
    Vec3 position,mins,maxs,head;
    int hp{},maxHp{};
    float meters{};
    bool visible{},visibilityKnown{},headHitbox{};
    std::string name;
};
struct Snapshot {
    std::vector<NpcRecord> npcs;
    bool inGame=false;
    float viewFov=90.0f;
    std::uint64_t updatedAt=0;
};
Settings ReadSettings();
void WriteSettings(Settings settings);
Snapshot ReadSnapshot();
void WriteSnapshot(Snapshot snapshot);
void SetStatus(std::string text);
std::string Status();
void SetDiagnostics(std::string text);
std::string Diagnostics();
void SetDirectory(std::wstring directory);
std::wstring OutputPath(const wchar_t* filename);
void Log(const std::string& text);
bool SaveSettings();
void LoadSettings();
}
