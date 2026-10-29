#pragma once
#include <cstdint>
#include <string_view>
namespace uw::profile {
inline constexpr const char* engineHash="6a277234ae8edb4939ed7ef45a27499210f14db3a7f6a9e7e83540492b638d6e";
inline constexpr std::uint32_t engineTimestamp=0x675781e6;
inline constexpr const char* inputHash="9566709efa9344dd5fce79dbc0d42e94bc302cd1f9c06daf241000f552c6c493";
inline constexpr std::uint32_t inputTimestamp=0x67453205;
inline constexpr const char* surfaceHash="fabbebc4bc8a29663a74c39ba54b7cf2c41aa802d65e26d50a5d8a745f88c47a";
inline constexpr std::uint32_t surfaceTimestamp=0x674532e7;
inline constexpr const char* physicsHash="66a74c0289cf67b1e2666bb3f3010320dfc91481969271c691944b963d2aa485";
inline constexpr std::uint32_t physicsTimestamp=0x674532f1;
struct GameBuild {const char* name;const char* folder;const char* serverHash;const char* clientHash;std::uintptr_t fireBullets,gameFrame,snapEyeAngles,revolverRecoilReturn,dataMapGetter,toolsPosition,toolsFirst,toolsNext,toolsKeyValue,toolsEntity;};
inline constexpr GameBuild games[]{
{"HL2 19307283","hl2","99f01211453b2c4a773ee81d73622b8b27d38b54483a4ae5ee48d33a38ec986b","96c5dd5e3d25ce8f8cc321d123b77dbfe8fa6aaef39df518786055f3cd507fa9",0x114830,0x187d60,0x207a50,0x40e95d,0x2e43d0,0x44d890,0x44d740,0x44da70,0x44d840,0x44d750},
{"HL2 Complete 19307283","hl2_complete","7ce0bb2567bdb6d06306cdc0b5c2d31ba176759727d5d0c9a0ca8c6829cf9b3e","f0b1a5ae4d758bc95a009bdf605b4fe481b5438f0a4cf22ef7f998afc14e8874",0x114c60,0x188050,0x207f60,0x40e90d,0x2e4800,0x44e550,0x44e400,0x44e730,0x44e500,0x44e410},
};
inline constexpr const GameBuild* FindGameBuild(std::string_view server,std::string_view client){for(const auto& game:games)if(server==game.serverHash&&client==game.clientHash)return &game;return nullptr;}
inline constexpr int toolsGetPlayerPosition=3,toolsGetPlayerFov=5,toolsFirstEntity=7,toolsNextEntity=8,toolsGetKeyValue=10,toolsGetEntity=20,engineWorldToScreen=36;
inline constexpr unsigned shotMask=0x46004003;
inline constexpr int surfacePropCount=2,surfaceData=5,surfaceMaterial=0x48;
inline constexpr int rayIsRay=0x40,rayIsSwept=0x41;
inline constexpr int dataMap=11,boneTransform=198,relation=296;
inline constexpr int studioHdr=0x44c,origin=0x27c,collision=0x148,hitboxSet=0x358;
}
