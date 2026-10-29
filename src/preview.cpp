#include "platform.hpp"
#include "ui.hpp"
#include <d3d9.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx9.h>
#include <vector>
#include <cstring>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace {
LRESULT CALLBACK WindowProc(HWND h,UINT m,WPARAM w,LPARAM l){
    if(ImGui::GetCurrentContext()&&ImGui_ImplWin32_WndProcHandler(h,m,w,l))return 1;
    if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);
}
bool Capture(IDirect3DDevice9* device,const std::wstring& path){
    IDirect3DSurface9 *back=nullptr,*copy=nullptr;
    if(FAILED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back)))return false;
    D3DSURFACE_DESC description{};back->GetDesc(&description);
    bool ok=false;
    if(SUCCEEDED(device->CreateOffscreenPlainSurface(description.Width,description.Height,description.Format,D3DPOOL_SYSTEMMEM,&copy,nullptr))&&SUCCEEDED(device->GetRenderTargetData(back,copy))){
        D3DLOCKED_RECT locked{};
        if(SUCCEEDED(copy->LockRect(&locked,nullptr,D3DLOCK_READONLY))){
            BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};
            info.biSize=sizeof(info);info.biWidth=description.Width;info.biHeight=-static_cast<LONG>(description.Height);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;info.biSizeImage=description.Width*description.Height*4;
            file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+info.biSizeImage;
            HANDLE out=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);DWORD written{};
            if(out!=INVALID_HANDLE_VALUE){
                ok=WriteFile(out,&file,sizeof(file),&written,nullptr)&&WriteFile(out,&info,sizeof(info),&written,nullptr);
                for(UINT y=0;y<description.Height&&ok;++y)ok=WriteFile(out,static_cast<unsigned char*>(locked.pBits)+y*locked.Pitch,description.Width*4,&written,nullptr)!=0;
                CloseHandle(out);
            }
            copy->UnlockRect();
        }
    }
    if(copy)copy->Release();back->Release();return ok;
}
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR,int){
    const bool fovTest=std::wcsstr(GetCommandLineW(),L"--fov-test")!=nullptr;
    const bool capture=std::wcsstr(GetCommandLineW(),L"--capture")!=nullptr;
    wchar_t filename[32768]{};GetModuleFileNameW(nullptr,filename,32768);std::wstring directory(filename);directory.resize(directory.find_last_of(L"\\/"));uw::SetDirectory(directory);
    WNDCLASSEXW wc{sizeof(wc)};wc.hInstance=instance;wc.lpfnWndProc=WindowProc;wc.lpszClassName=L"UNJIWARE_PREVIEW";wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));RegisterClassExW(&wc);
    RECT rect{0,0,960,640};AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
    HWND window=CreateWindowExW(0,wc.lpszClassName,L"UNJIWARE / UI Preview",WS_OVERLAPPEDWINDOW,100,100,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,instance,nullptr);
    IDirect3D9* d3d=Direct3DCreate9(D3D_SDK_VERSION);IDirect3DDevice9* device=nullptr;
    D3DPRESENT_PARAMETERS p{};p.Windowed=TRUE;p.SwapEffect=D3DSWAPEFFECT_DISCARD;p.hDeviceWindow=window;p.BackBufferFormat=D3DFMT_A8R8G8B8;p.BackBufferWidth=960;p.BackBufferHeight=640;p.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    if(!d3d||FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&p,&device))){uw::Log("Preview D3D9 device creation failed");return 2;}
    if(!capture)ShowWindow(window,SW_SHOW);
    ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;uw::InitializeUiFonts();ImGui_ImplWin32_Init(window);ImGui_ImplDX9_Init(device);
    if(fovTest){uw::Settings c;c.silentAim=false;c.drawFov=true;uw::WriteSettings(c);}
    bool open=!fovTest,done=false;int frames=0,result=0;
    while(!done){
        MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);if(msg.message==WM_QUIT)done=true;}
        if(done)break;
        if(GetAsyncKeyState(VK_INSERT)&1)open=!open;
        ImGui_ImplDX9_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();if(fovTest)uw::DrawFov(960,640);uw::DrawUi(open,true);ImGui::Render();
        device->Clear(0,nullptr,D3DCLEAR_TARGET,D3DCOLOR_XRGB(13,20,29),1,0);
        if(SUCCEEDED(device->BeginScene())){ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());device->EndScene();}
        if(capture&&++frames==8){result=Capture(device,uw::OutputPath(fovTest?L"fov-preview.bmp":L"ui-preview.bmp"))?0:3;done=true;}
        device->Present(nullptr,nullptr,nullptr,nullptr);if(!capture)Sleep(10);
    }
    ImGui_ImplDX9_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();device->Release();d3d->Release();DestroyWindow(window);UnregisterClassW(wc.lpszClassName,instance);return result;
}
