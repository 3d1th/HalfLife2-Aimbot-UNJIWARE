#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <MinHook.h>
#include "platform.hpp"
#include "profile.hpp"
#include "game.hpp"
#include "state.hpp"

struct Map;
struct Field{int type;const char* name;int offsets[2];unsigned short size;short flags;const char* external;void* save;void* function;Map* child;int bytes;void* overrideField;int overrideCount;float tolerance;};
struct Map{Field* fields;int count;const char* name;Map* parent;};
using Getter=Map*(__thiscall*)(void*);
Getter originalGetter=nullptr;
int hookCalls=0;
Map* __fastcall ProbeHook(void* self,void*){++hookCalls;return originalGetter(self);}
struct RecvTable;
struct RecvProp{const char* name;int type,flags,stringSize;bool inArray;void* extra;RecvProp* array;void* lengthProxy;void* variableProxy;void* tableProxy;RecvTable* child;int offset,stride,count;const char* parent;};
struct RecvTable{RecvProp* props;int count;void* decoder;const char* name;};
struct ClientClass{void* create;void* event;const char* name;RecvTable* table;ClientClass* next;int id;};
int Netvar(RecvTable* table,const char* name,int base=0,int depth=0){
    if(!table||depth>20||table->count<0||table->count>2048)return -1;
    for(int i=0;i<table->count;++i){auto& p=table->props[i];if(p.name&&std::strcmp(p.name,name)==0)return base+p.offset;if(p.child){int found=Netvar(p.child,name,base+p.offset,depth+1);if(found>=0)return found;}}
    return -1;
}
int Find(Map* map,const char* name,int base=0,int depth=0){
    if(!map||depth>20||map->count<0||map->count>2048)return -1;
    for(int i=0;i<map->count;++i){auto& f=map->fields[i];if(f.name&&std::strcmp(f.name,name)==0)return base+f.offsets[0];if(f.type==10&&!(f.flags&0x40)){int found=Find(f.child,name,base+f.offsets[0],depth+1);if(found>=0)return found;}}
    return Find(map->parent,name,base,depth+1);
}
int main(int argc,char** argv){
    const bool legacy=argc>1&&std::strcmp(argv[1],"--legacy")==0;
    const auto& profile=uw::profile::games[legacy?0:1];
    wchar_t configured[32768]{};DWORD length=GetEnvironmentVariableW(L"UW_HL2_DIR",configured,32768);
    std::wstring directory=(length>0&&length<32768)?configured:L"D:\\SteamLibrary\\steamapps\\common\\Half-Life 2";
    if(directory.back()!=L'\\'&&directory.back()!=L'/')directory+=L'\\';
    const std::wstring variant=legacy?L"hl2":L"hl2_complete";
    SetDllDirectoryW((directory+L"bin").c_str());
    std::printf("Checking %s\n",profile.name);
    HMODULE server=LoadLibraryExW((directory+variant+L"\\bin\\server.dll").c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    if(!server){std::printf("LoadLibrary server.dll failed: %lu\n",GetLastError());return 2;}
    auto base=reinterpret_cast<std::uintptr_t>(server);
    // Offline inspection proves this pinned-build function is `mov eax, imm32; ret`.
    Map* map=reinterpret_cast<Getter>(base+profile.dataMapGetter)(nullptr);
    if(!map||!map->name){std::puts("Datamap not initialized");return 3;}
    std::printf("Root: %s, fields=%d, descriptor_size=%zu\n",map->name,map->count,sizeof(Field));
    int failed=0;
    const char* names[]{"m_iHealth","m_lifeState","m_vecPunchAngle","m_vecPunchAngleVel"};
    for(const char* name:names){int value=Find(map,name);std::printf("%s = 0x%x (%d)\n",name,value,value);if(value<0)++failed;}
    HMODULE client=LoadLibraryExW((directory+variant+L"\\bin\\client.dll").c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    if(!client){std::printf("LoadLibrary client.dll failed: %lu\n",GetLastError());return 2;}
    std::string failure;
    auto serverHash=uw::FileHash(server,&failure);if(!failure.empty())std::printf("Server hash error: %s\n",failure.c_str());
    auto clientHash=uw::FileHash(client,&failure);if(!failure.empty())std::printf("Client hash error: %s\n",failure.c_str());
    auto* selected=uw::profile::FindGameBuild(serverHash,clientHash);
    if(selected!=&profile){std::puts("Runtime SHA256/profile matching failed");++failed;}
    else std::printf("Runtime SHA256/profile: %s\n",selected->name);
    if(uw::profile::FindGameBuild(uw::profile::games[0].serverHash,uw::profile::games[1].clientHash)||uw::profile::FindGameBuild("unknown",clientHash)){std::puts("Negative profile checks failed");++failed;}
    uw::FileHash(nullptr,&failure);if(failure.empty()){std::puts("Missing module error was not distinguished");++failed;}
    auto factory=reinterpret_cast<void*(__cdecl*)(const char*,int*)>(GetProcAddress(client,"CreateInterface"));
    void* dll=factory?factory("VClient017",nullptr):nullptr;
    if(!dll){std::puts("VClient017 unavailable");return 2;}
    using Classes=ClientClass*(__thiscall*)(void*);
    ClientClass* cc=reinterpret_cast<Classes>((*reinterpret_cast<void***>(dll))[8])(dll);
    bool foundTable=false;
    for(int i=0;cc&&i++<2048;cc=cc->next){
        if(!cc->table||std::strcmp(cc->table->name,"DT_BasePlayer")!=0)continue;
        int punch=Netvar(cc->table,"m_vecPunchAngle[0]"),velocity=Netvar(cc->table,"m_vecPunchAngleVel[0]");
        if(punch<0)punch=Netvar(cc->table,"m_vecPunchAngle");if(velocity<0)velocity=Netvar(cc->table,"m_vecPunchAngleVel");
        std::printf("Client punch = 0x%x, velocity = 0x%x, descriptor_size=%zu\n",punch,velocity,sizeof(RecvProp));
        if(punch<0||velocity<0)++failed;foundTable=true;break;
    }
    if(!foundTable){std::puts("DT_BasePlayer missing");++failed;}
    void* target=reinterpret_cast<void*>(base+profile.dataMapGetter);
    if(MH_Initialize()==MH_OK&&MH_CreateHook(target,reinterpret_cast<void*>(&ProbeHook),reinterpret_cast<void**>(&originalGetter))==MH_OK&&MH_EnableHook(target)==MH_OK){
        Map* again=reinterpret_cast<Getter>(target)(nullptr);
        if(again!=map||hookCalls!=1)++failed;
        std::printf("MinHook ABI smoke: calls=%d, returned_map_matches=%d\n",hookCalls,again==map);
    }else{std::puts("MinHook ABI smoke failed");++failed;}
    MH_DisableHook(target);MH_RemoveHook(target);
    HMODULE engine=LoadLibraryExW((directory+L"bin\\engine.dll").c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    LoadLibraryExW((directory+L"bin\\inputsystem.dll").c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    HMODULE physics=LoadLibraryExW((directory+L"bin\\vphysics.dll").c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    void* surface=uw::Interface(L"vphysics.dll","VPhysicsSurfaceProps001");
    if(!physics||uw::FileHash(physics)!=uw::profile::physicsHash||!surface){std::puts("Physics surface ABI unavailable");++failed;}
    else std::printf("Physics surfaces: count=%d\n",uw::VCall<int>(surface,uw::profile::surfacePropCount));
    auto output=uw::ModulePath(GetModuleHandleW(nullptr));output.resize(output.find_last_of(L"\\/"));output+=L"\\probe-logs";CreateDirectoryW(output.c_str(),nullptr);uw::SetDirectory(output);uw::WriteSettings(uw::Settings{});
    if(!engine||!uw::InitializeGame()||uw::Status()!=std::string(profile.name)+" / ready (R7)"){std::printf("Startup validation failed: %s\n",uw::Status().c_str());++failed;}
    else std::printf("Startup validation: %s\n",uw::Status().c_str());
    MH_DisableHook(MH_ALL_HOOKS);MH_Uninitialize();
    std::puts(failed?"ABI PROBE FAILED":"ABI PROBE PASSED");
    // Process termination unloads the isolated copy; no game process is touched.
    return failed?1:0;
}
