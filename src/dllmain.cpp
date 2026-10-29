#include "platform.hpp"
#include "renderer.hpp"
#include "game.hpp"
#include "state.hpp"
#include <MinHook.h>

namespace {
DWORD WINAPI Start(void* module){
    wchar_t path[32768]{};GetModuleFileNameW(static_cast<HMODULE>(module),path,32768);
    std::wstring directory(path);directory.resize(directory.find_last_of(L"\\/"));uw::SetDirectory(directory);uw::LoadSettings();uw::Log("UNJIWARE x86 attached");
    if(MH_Initialize()!=MH_OK){uw::SetStatus("MinHook initialization failed");return 1;}
    bool renderer=false,game=false;
    for(int i=0;i<1200&&(!renderer||!game);++i){
        if(!renderer&&GetModuleHandleW(L"shaderapidx9.dll"))renderer=uw::InitializeRenderer(static_cast<HMODULE>(module));
        if(!game)game=uw::InitializeGame();
        if(!renderer||!game)Sleep(100);
    }
    if(!renderer)uw::Log("Direct3D 9 not ready. Use the default DX9 renderer; Vulkan is not supported.");
    return 0;
}
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);HANDLE thread=CreateThread(nullptr,0,Start,module,0,nullptr);if(thread)CloseHandle(thread);}
    // Hooks live until process exit. No unsafe cleanup under the loader lock.
    return TRUE;
}
