#include "platform.hpp"
#include "renderer.hpp"
#include "game.hpp"
#include "ui.hpp"
#include "profile.hpp"
#include <d3d9.h>
#include <MinHook.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx9.h>
#include <atomic>
#include <mutex>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace uw {
namespace {
using EndSceneFn=HRESULT(WINAPI*)(IDirect3DDevice9*);
using ResetFn=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
using LockFn=void(__thiscall*)(void*);
EndSceneFn originalEnd=nullptr;ResetFn originalReset=nullptr;LockFn originalLock=nullptr;
HWND window=nullptr;WNDPROC originalWndProc=nullptr;IDirect3DDevice9* activeDevice=nullptr;
std::recursive_mutex rendering;
std::atomic<bool> menu{true};bool ready=false,wasInsert=false;
void* surface=nullptr;
void __fastcall LockHook(void* self,void*){
    if(menu&&ready){VCall<void>(self,61);ClipCursor(nullptr);return;}originalLock(self);
}
LRESULT CALLBACK WndProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam){
    std::lock_guard lock(rendering);
    if(ready&&ImGui::GetCurrentContext()){
        ImGui_ImplWin32_WndProcHandler(hwnd,message,wparam,lparam);
        if(menu&&(message==WM_MOUSEMOVE||message==WM_MOUSEWHEEL||message==WM_MOUSEHWHEEL||message==WM_LBUTTONDOWN||message==WM_LBUTTONUP||message==WM_RBUTTONDOWN||message==WM_RBUTTONUP||message==WM_MBUTTONDOWN||message==WM_MBUTTONUP||message==WM_XBUTTONDOWN||message==WM_XBUTTONUP||message==WM_INPUT||message==WM_KEYDOWN||message==WM_KEYUP||message==WM_CHAR)){
            if(message==WM_INPUT)return DefWindowProcW(hwnd,message,wparam,lparam);
            return 0;
        }
    }
    return CallWindowProcW(originalWndProc,hwnd,message,wparam,lparam);
}
bool Setup(IDirect3DDevice9* device){
    D3DDEVICE_CREATION_PARAMETERS p{};if(FAILED(device->GetCreationParameters(&p)))return false;
    DWORD process{};GetWindowThreadProcessId(p.hFocusWindow,&process);
    if(process!=GetCurrentProcessId()||!IsWindowVisible(p.hFocusWindow))return false;
    window=p.hFocusWindow;activeDevice=device;IMGUI_CHECKVERSION();ImGui::CreateContext();
    ImGui::GetIO().IniFilename=nullptr;InitializeUiFonts();
    if(!ImGui_ImplWin32_Init(window)||!ImGui_ImplDX9_Init(device)){Log("ImGui backend initialization failed");return false;}
    SetLastError(0);originalWndProc=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&WndProc)));
    if(!originalWndProc){Log("Window procedure hook failed");return false;}
    ready=true;Log("Direct3D 9 UI initialized");return true;
}
HRESULT WINAPI EndSceneHook(IDirect3DDevice9* device){
    {
        std::lock_guard lock(rendering);
        if((ready&&activeDevice==device)||(!ready&&Setup(device))){
            DWORD process{};GetWindowThreadProcessId(GetForegroundWindow(),&process);
            bool insert=(GetAsyncKeyState(VK_INSERT)&0x8000)!=0;
            if(insert&&!wasInsert&&process==GetCurrentProcessId())menu=!menu.load();wasInsert=insert;
            bool capture=menu&&process==GetCurrentProcessId();
            CaptureGameInput(capture);
            if(capture){ClipCursor(nullptr);if(surface)VCall<void>(surface,61);}
            ImGui::GetIO().MouseDrawCursor=capture;
            ImGui_ImplDX9_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
            Matrix4x4 matrix{};bool projected=GameMatrix(matrix);
            const auto size=ImGui::GetIO().DisplaySize;
            DrawFov(size.x,size.y,projected?&matrix:nullptr);
            if(projected)DrawEsp(matrix,size.x,size.y);
            GameHeartbeat();
            bool open=menu;DrawUi(open);menu=open;
            ImGui::EndFrame();ImGui::Render();ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        }
    }
    return originalEnd(device);
}
HRESULT WINAPI ResetHook(IDirect3DDevice9* device,D3DPRESENT_PARAMETERS* p){
    std::lock_guard lock(rendering);
    if(ready&&activeDevice==device)ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT result=originalReset(device,p);
    if(SUCCEEDED(result)&&ready&&activeDevice==device)ImGui_ImplDX9_CreateDeviceObjects();return result;
}
}
bool InitializeRenderer(HMODULE own){
    WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=own;wc.lpszClassName=L"UNJIWARE_D3D_PROBE";
    if(!RegisterClassExW(&wc))return false;
    HWND probe=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,64,64,nullptr,nullptr,own,nullptr);
    IDirect3D9* d3d=Direct3DCreate9(D3D_SDK_VERSION);IDirect3DDevice9* device=nullptr;
    D3DPRESENT_PARAMETERS p{};p.Windowed=TRUE;p.SwapEffect=D3DSWAPEFFECT_DISCARD;p.hDeviceWindow=probe;p.BackBufferFormat=D3DFMT_UNKNOWN;
    bool ok=false;
    if(d3d&&SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,probe,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&p,&device))){
        void** vtable=*reinterpret_cast<void***>(device);
        if(MH_CreateHook(vtable[42],reinterpret_cast<void*>(&EndSceneHook),reinterpret_cast<void**>(&originalEnd))==MH_OK&&MH_CreateHook(vtable[16],reinterpret_cast<void*>(&ResetHook),reinterpret_cast<void**>(&originalReset))==MH_OK){
            ok=MH_EnableHook(vtable[42])==MH_OK&&MH_EnableHook(vtable[16])==MH_OK;
        }
        device->Release();
    }
    if(d3d)d3d->Release();DestroyWindow(probe);UnregisterClassW(wc.lpszClassName,own);
    HMODULE surfaceModule=GetModuleHandleW(L"vguimatsurface.dll");
    if(surfaceModule&&FileHash(surfaceModule)==profile::surfaceHash){
        surface=Interface(L"vguimatsurface.dll","VGUI_Surface030");
        if(surface){void* lock=(*reinterpret_cast<void***>(surface))[62];if(MH_CreateHook(lock,reinterpret_cast<void*>(&LockHook),reinterpret_cast<void**>(&originalLock))==MH_OK)MH_EnableHook(lock);}
    }
    return ok;
}
}
