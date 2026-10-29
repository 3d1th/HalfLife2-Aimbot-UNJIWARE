#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "state.hpp"
#include <cstdio>
#include <utility>

namespace uw {
namespace {
std::mutex stateMutex,logMutex;
Settings config;
Snapshot frame;
std::string currentStatus="Waiting for game interfaces";
std::wstring directory;
std::string diagnostics="Waiting for runtime callbacks";
}
Settings ReadSettings(){std::lock_guard lock(stateMutex);return config;}
void WriteSettings(Settings s){
    s.visibleOnly=true;s.espVisibility=true; // Wall checks and colors stay on, including legacy INI values.
    if(!std::isfinite(s.aimFov))s.aimFov=9;
    if(!std::isfinite(s.maxDistance))s.maxDistance=100;
    s.aimFov=std::clamp(s.aimFov,1.0f,45.0f);
    s.maxDistance=std::clamp(s.maxDistance,5.0f,500.0f);
    s.activation=std::clamp(s.activation,0,1);
    for(auto& c:s.accent)c=std::isfinite(c)?std::clamp(c,0.0f,1.0f):0.5f;
    std::lock_guard lock(stateMutex);config=s;
}
Snapshot ReadSnapshot(){std::lock_guard lock(stateMutex);return frame;}
void WriteSnapshot(Snapshot s){std::lock_guard lock(stateMutex);frame=std::move(s);}
void SetStatus(std::string s){std::lock_guard lock(stateMutex);currentStatus=std::move(s);}
std::string Status(){std::lock_guard lock(stateMutex);return currentStatus;}
void SetDiagnostics(std::string s){std::lock_guard lock(stateMutex);diagnostics=std::move(s);}
std::string Diagnostics(){std::lock_guard lock(stateMutex);return diagnostics;}
void SetDirectory(std::wstring s){directory=std::move(s);}
std::wstring OutputPath(const wchar_t* name){return directory+L"\\"+name;}
void Log(const std::string& text){
    std::lock_guard lock(logMutex);
    HANDLE f=CreateFileW(OutputPath(L"UNJIWARE.log").c_str(),FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE)return;
    SYSTEMTIME t{};GetLocalTime(&t);char stamp[48];
    std::snprintf(stamp,sizeof(stamp),"[%02d:%02d:%02d] ",t.wHour,t.wMinute,t.wSecond);
    std::string line=stamp+text+"\r\n";DWORD written{};
    WriteFile(f,line.data(),static_cast<DWORD>(line.size()),&written,nullptr);CloseHandle(f);
}
bool SaveSettings(){
    auto s=ReadSettings();auto file=OutputPath(L"UNJIWARE.ini");bool ok=true;
    auto put=[&](const wchar_t* key,int value){ok=WritePrivateProfileStringW(L"settings",key,std::to_wstring(value).c_str(),file.c_str())&&ok;};
#define UW_WIDE2(x) L##x
#define UW_WIDE(x) UW_WIDE2(x)
#define UW_SAVE(x) put(UW_WIDE(#x),s.x)
    UW_SAVE(esp);UW_SAVE(boxes);UW_SAVE(names);UW_SAVE(health);UW_SAVE(distance);UW_SAVE(espVisibility);
    UW_SAVE(combine);UW_SAVE(zombies);UW_SAVE(antlions);UW_SAVE(headcrabs);UW_SAVE(other);
    UW_SAVE(silentAim);UW_SAVE(visibleOnly);UW_SAVE(drawFov);UW_SAVE(noSpread);UW_SAVE(noRecoil);UW_SAVE(watermark);UW_SAVE(activation);
#undef UW_SAVE
    put(L"aimFov100",static_cast<int>(s.aimFov*100));put(L"maxDistance100",static_cast<int>(s.maxDistance*100));
    for(int i=0;i<3;++i)put((L"accent"+std::to_wstring(i)).c_str(),static_cast<int>(s.accent[i]*1000));
    return ok;
}
void LoadSettings(){
    Settings s;auto file=OutputPath(L"UNJIWARE.ini");
    auto get=[&](const wchar_t* key,int value){return static_cast<int>(GetPrivateProfileIntW(L"settings",key,value,file.c_str()));};
#define UW_LOAD(x) s.x=get(UW_WIDE(#x),s.x)!=0
    UW_LOAD(esp);UW_LOAD(boxes);UW_LOAD(names);UW_LOAD(health);UW_LOAD(distance);UW_LOAD(espVisibility);
    UW_LOAD(combine);UW_LOAD(zombies);UW_LOAD(antlions);UW_LOAD(headcrabs);UW_LOAD(other);
    UW_LOAD(silentAim);UW_LOAD(visibleOnly);UW_LOAD(drawFov);UW_LOAD(noSpread);UW_LOAD(noRecoil);UW_LOAD(watermark);
#undef UW_LOAD
    s.activation=get(L"activation",0);s.aimFov=get(L"aimFov100",900)/100.0f;s.maxDistance=get(L"maxDistance100",10000)/100.0f;
    for(int i=0;i<3;++i)s.accent[i]=get((L"accent"+std::to_wstring(i)).c_str(),static_cast<int>(s.accent[i]*1000))/1000.0f;
    WriteSettings(s);
}
}
