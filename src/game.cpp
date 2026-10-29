#include "platform.hpp"
#include "game.hpp"
#include "profile.hpp"
#include "state.hpp"
#include <MinHook.h>
#include <cstring>
#include <optional>
#include <unordered_map>
#include <atomic>
#include <cstdio>
#include <chrono>
#include <cstddef>

namespace uw {
namespace {
void *engine=nullptr,*tools=nullptr,*trace=nullptr,*entityList=nullptr,*physicsProps=nullptr;
std::atomic<void*> input{nullptr};
std::uintptr_t serverAddress=0;
const profile::GameBuild* activeBuild=nullptr;
std::atomic<bool> initialized{false};
enum class Gate {Waiting,NotInGame,NotSinglePlayer,NoIndex,NoPlayer,Ready,Dead};
std::atomic<Gate> gate{Gate::Waiting};
std::atomic<unsigned> frameCalls{0},stageCalls{0},bulletCalls{0},playerShots{0},aimedShots{0};
std::atomic<int> localIndex{0},maxClients{0};
std::atomic<unsigned> visibilityChecks{0},visibilityBlocked{0},visibilityInvalid{0};
const char* GateName(Gate value){
    switch(value){
    case Gate::Waiting:return "waiting for server frame";
    case Gate::NotInGame:return "not in game";
    case Gate::NotSinglePlayer:return "not singleplayer";
    case Gate::NoIndex:return "no local index";
    case Gate::NoPlayer:return "no server player";
    case Gate::Ready:return "player found";
    case Gate::Dead:return "player not alive";
    }return "unknown";
}
struct DataMap;
struct DataField {int type;const char* name;int offset[2];unsigned short size;short flags;const char* external;void* save;void* function;DataMap* embedded;int bytes;void* overrideField;int overrideCount;float tolerance;};
struct DataMap {DataField* fields;int count;const char* name;DataMap* parent;};
static_assert(sizeof(DataField)==52);
struct Fields {int health=-1,life=-1,punch=-1,punchVelocity=-1,spawnFlags=-1;};
std::unordered_map<DataMap*,Fields> fieldCache;
std::unordered_map<DataMap*,bool> npcClassCache;
struct VisibilityEntry {ULONGLONG checked=0,seen=0;bool known=false,visible=false;};
std::unordered_map<void*,VisibilityEntry> visibilityCache;
ULONGLONG nextEspUpdate=0;
bool espWasEnabled=false;
std::size_t visibilityCursor=0;
std::atomic<unsigned> espUpdates{0},espLastUs{0},espPeakUs{0};
struct EspTimer {
    bool enabled;std::chrono::steady_clock::time_point started;
    explicit EspTimer(bool on):enabled(on),started(std::chrono::steady_clock::now()){}
    ~EspTimer(){
        if(!enabled)return;
        const auto us=static_cast<unsigned>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count());
        ++espUpdates;espLastUs=us;if(us>espPeakUs.load())espPeakUs=us;
    }
};
bool StringIs(const char* s,const char* expected){return Readable(s,std::strlen(expected)+1)&&std::strcmp(s,expected)==0;}
std::optional<int> FindField(DataMap* map,const char* name,int base=0,int depth=0){
    if(depth>20||!Readable(map,sizeof(DataMap))||map->count<0||map->count>2048)return {};
    if(Readable(map->fields,map->count*sizeof(DataField)))for(int i=0;i<map->count;++i){auto& f=map->fields[i];
        if(f.offset[0]<0||f.offset[0]>0x20000)continue;
        if(StringIs(f.name,name))return base+f.offset[0];
        if(f.type==10&&!(f.flags&0x40)){auto found=FindField(f.embedded,name,base+f.offset[0],depth+1);if(found)return found;}
    }
    return FindField(map->parent,name,base,depth+1);
}
DataMap* Map(void* e){return VCall<DataMap*>(e,profile::dataMap);}
bool IsNpc(DataMap* map){
    if(auto it=npcClassCache.find(map);it!=npcClassCache.end())return it->second;
    auto* root=map;bool npc=false;
    for(int i=0;i<24&&Readable(map,sizeof(DataMap));++i,map=map->parent)
        if(StringIs(map->name,"CAI_BaseNPC")){npc=true;break;}
    npcClassCache.emplace(root,npc);return npc;
}
Fields EntityFields(void* e){
    auto* map=Map(e);auto it=fieldCache.find(map);if(it!=fieldCache.end())return it->second;
    Fields f;f.health=FindField(map,"m_iHealth").value_or(-1);f.life=FindField(map,"m_lifeState").value_or(-1);
    f.punch=FindField(map,"m_vecPunchAngle").value_or(-1);f.punchVelocity=FindField(map,"m_vecPunchAngleVel").value_or(-1);
    f.spawnFlags=FindField(map,"m_spawnflags").value_or(-1);
    fieldCache[map]=f;
    Log("Datamap fields: health="+std::to_string(f.health)+", life="+std::to_string(f.life)+", punch="+std::to_string(f.punch)+", punch_velocity="+std::to_string(f.punchVelocity));
    return f;
}
void* LocalPlayer(){
    if(!engine||!tools||!VCall<bool>(engine,26)){gate=Gate::NotInGame;return nullptr;}
    int clients=VCall<int>(engine,21);maxClients=clients;
    if(clients!=1){gate=Gate::NotSinglePlayer;return nullptr;}
    int index=VCall<int>(engine,12);localIndex=index;
    if(index<=0){gate=Gate::NoIndex;return nullptr;}
    void* player=VCall<void*>(tools,profile::toolsGetEntity,index);
    gate=player?Gate::Ready:Gate::NoPlayer;return player;
}
bool Alive(void* e){auto f=EntityFields(e);return f.health>=0&&f.life>=0&&Field<int>(e,f.health)>0&&Field<unsigned char>(e,f.life)==0;}
void ClearPunch(void* player){if(!player)return;auto f=EntityFields(player);if(f.punch>=0)Field<Vec3>(player,f.punch)={};if(f.punchVelocity>=0)Field<Vec3>(player,f.punchVelocity)={};}
bool Category(const char* name,const Settings& c){
    if(std::strstr(name,"zombie")||std::strstr(name,"zombine"))return c.zombies;
    if(std::strstr(name,"headcrab"))return c.headcrabs;
    if(std::strstr(name,"antlion"))return c.antlions;
    if(std::strstr(name,"combine")||std::strstr(name,"metropolice")||std::strstr(name,"hunter")||std::strstr(name,"manhack")||std::strstr(name,"scanner")||std::strstr(name,"strider")||std::strstr(name,"helicopter")||std::strstr(name,"stalker"))return c.combine;
    return c.other;
}
bool IsCombatNpcClass(const char* name){
    // Script targets and ambient birds can carry hostile relationships without being combat enemies.
    if(std::strncmp(name,"npc_enemyfinder",15)==0)return false;
    const char* excluded[]{"npc_bullseye","npc_crow","npc_pigeon","npc_seagull","npc_furniture","npc_generic",
        "npc_maker","npc_template_maker","npc_launcher"};
    for(const auto* value:excluded)if(std::strcmp(name,value)==0)return false;
    return std::strncmp(name,"npc_",4)==0;
}
bool Enemy(void* npc,void* player,const Settings& c,char(&name)[128]){
    if(npc==player||!IsNpc(Map(npc))||!Alive(npc))return false;
    if(!VCall<bool>(tools,profile::toolsGetKeyValue,npc,static_cast<const char*>("classname"),static_cast<char*>(name),128))return false;
    name[127]=0;
    return IsCombatNpcClass(name)&&Category(name,c)&&VCall<int>(npc,profile::relation,player)==1;
}
bool AimPoint(void* npc,Vec3& point,bool& isHead){
    Matrix3x4 matrix{};
    // GetBoneTransform initializes the model/bone cache if necessary.
    VCall<void>(npc,profile::boneTransform,0,&matrix);
    void* wrapper=Field<void*>(npc,profile::studioHdr);
    if(!Readable(wrapper,4))return false;
    auto* hdr=Field<unsigned char*>(wrapper,0);
    if(!Readable(hdr,180)||Field<int>(hdr,0)!=0x54534449)return false;
    const int length=Field<int>(hdr,76),bones=Field<int>(hdr,156),sets=Field<int>(hdr,172),setOffset=Field<int>(hdr,176);
    int set=Field<int>(npc,profile::hitboxSet);
    if(length<180||length>0x1000000||bones<=0||bones>256||sets<=0||sets>64||set<0||set>=sets||setOffset<180||setOffset+sets*12>length)return false;
    auto* boxes=hdr+setOffset+set*12;if(!Readable(boxes,12))return false;
    int count=Field<int>(boxes,4),offset=Field<int>(boxes,8);
    if(count<=0||count>256||offset<12||boxes-hdr+offset+count*68>length||!Readable(boxes+offset,count*68))return false;
    unsigned char* selected=nullptr;isHead=false;
    for(int i=0;i<count;++i){auto* box=boxes+offset+i*68;int bone=Field<int>(box,0);if(bone<0||bone>=bones)continue;if(!selected)selected=box;if(Field<int>(box,4)==1){selected=box;isHead=true;break;}}
    if(!selected)return false;
    Vec3 center=(Field<Vec3>(selected,8)+Field<Vec3>(selected,20))*0.5f;
    VCall<void>(npc,profile::boneTransform,Field<int>(selected,0),&matrix);
    point=matrix.Transform(center);return point.Finite();
}
struct alignas(16) Ray {float start[4]{},delta[4]{},offset[4]{},extents[4]{};bool ray=true,swept=false;};
struct Trace {Vec3 start,end;float plane[5];float fraction;int contents;unsigned short flags;bool allSolid,startSolid;float left;const char* surfaceName;short surfaceProps;unsigned short surfaceFlags;int hitgroup;short physicsBone;unsigned short worldSurface;void* entity;int hitbox;};
static_assert(sizeof(Ray)==80&&sizeof(Trace)==84);
static_assert(offsetof(Ray,ray)==profile::rayIsRay&&offsetof(Ray,swept)==profile::rayIsSwept);
static_assert(offsetof(Trace,fraction)==0x2c&&offsetof(Trace,startSolid)==0x37&&offsetof(Trace,entity)==0x4c);
struct TraceFilter {
    void *skip,*extra;
    explicit TraceFilter(void* e,void* additional=nullptr):skip(e),extra(additional){}
    virtual bool __thiscall ShouldHit(void* e,int){return e!=skip&&e!=extra;}
    virtual int __thiscall Type()const{return 0;}
};
bool TraceShot(Vec3 start,Vec3 end,void* player,Trace& out,void* extra=nullptr){
    ++visibilityChecks;
    if(!trace||!player||!start.Finite()||!end.Finite()){++visibilityInvalid;++visibilityBlocked;return false;}
    Ray r;const Vec3 delta=end-start;
    if(delta.Dot(delta)<0.000001f){++visibilityInvalid;++visibilityBlocked;return false;}
    std::memcpy(r.start,&start,12);std::memcpy(r.delta,&delta,12);r.swept=true;
    TraceFilter filter(player,extra);VCall<void>(trace,4,&r,profile::shotMask,&filter,&out);
    const bool valid=std::isfinite(out.fraction)&&out.fraction>=0&&out.fraction<=1;
    if(!valid){++visibilityInvalid;++visibilityBlocked;}return valid;
}
bool ClearShot(const Trace& result,void* target){
    return !result.startSolid&&!result.allSolid&&(result.entity==target||result.fraction==1.0f);
}
bool Visible(Vec3 start,Vec3 end,void* player,void* target){
    Trace out{};if(!target||!TraceShot(start,end,player,out))return false;
    const bool visible=ClearShot(out,target);if(!visible)++visibilityBlocked;return visible;
}
bool PenetrableGlass(const Trace& hit){
    if(!physicsProps||!hit.entity||hit.surfaceProps<0)return false;
    const int count=VCall<int>(physicsProps,profile::surfacePropCount);
    if(count<0||count>65536||hit.surfaceProps>=count)return false;
    void* data=VCall<void*>(physicsProps,profile::surfaceData,static_cast<int>(hit.surfaceProps));
    if(!Readable(data,profile::surfaceMaterial+2)||Field<unsigned short>(data,profile::surfaceMaterial)!='Y')return false;
    char name[128]{};
    if(!VCall<bool>(tools,profile::toolsGetKeyValue,hit.entity,static_cast<const char*>("classname"),static_cast<char*>(name),128))return false;
    name[127]=0;if(std::strcmp(name,"func_breakable")!=0)return false;
    const auto fields=EntityFields(hit.entity);
    return fields.spawnFlags>=0&&(Field<int>(hit.entity,fields.spawnFlags)&0x0800)==0;
}
bool CanHitTarget(Vec3 start,Vec3 end,void* player,void* target,void* extra=nullptr){
    if(!target||target==extra)return false;
    const Vec3 direction=(end-start).Normalized();
    for(int layers=0;layers<=8;++layers){
        Trace entry{};if(!TraceShot(start,end,player,entry,extra))return false;
        if(ClearShot(entry,target))return true;
        ++visibilityBlocked;
        if(layers==8||entry.startSolid||entry.allSolid||entry.fraction<=0||!entry.end.Finite()||!PenetrableGlass(entry))return false;
        // Match HL2's 16-unit reverse trace. Ambiguous exits remain blocked.
        Trace exit{};const Vec3 probe=entry.end+direction*16.0f;
        if(!TraceShot(probe,entry.end,player,exit,extra)||exit.startSolid||exit.allSolid||exit.fraction>=1
            ||exit.entity!=entry.entity||!exit.end.Finite())return false;
        if((exit.end-start).Dot(direction)<=0.001f||(end-exit.end).Dot(direction)<=0)return false;
        start=exit.end;
    }
    return false;
}
void UpdateAt(ULONGLONG now){
    const auto c=ReadSettings();
    // Snapshot collection runs at most 30 Hz. Drawing keeps the render frame rate.
    if(now<nextEspUpdate&&c.esp==espWasEnabled)return;
    nextEspUpdate=now+33;espWasEnabled=c.esp;
    EspTimer timing(c.esp);
    Snapshot s;s.updatedAt=now;void* player=LocalPlayer();
    if(!player||!Alive(player)){
        if(player)gate=Gate::Dead;
        visibilityCache.clear();WriteSnapshot(std::move(s));return;
    }
    s.inGame=true;
    s.viewFov=static_cast<float>(VCall<int>(tools,profile::toolsGetPlayerFov,static_cast<void*>(nullptr)));
    if(!c.esp){visibilityCache.clear();WriteSnapshot(std::move(s));return;}
    Vec3 playerPos=Field<Vec3>(player,profile::origin),eye{},angles{};
    if(c.espVisibility&&!VCall<bool>(tools,profile::toolsGetPlayerPosition,&eye,&angles,static_cast<void*>(nullptr)))
        eye=playerPos+Vec3{0,0,64};
    std::vector<void*> targets;
    s.npcs.reserve(32);if(c.espVisibility)targets.reserve(32);
    int visited=0;
    for(void* npc=VCall<void*>(tools,profile::toolsFirstEntity);npc&&visited++<8192;npc=VCall<void*>(tools,profile::toolsNextEntity,npc)){
        char name[128]{};if(!Enemy(npc,player,c,name))continue;
        NpcRecord n;n.position=Field<Vec3>(npc,profile::origin);
        const auto delta=n.position-playerPos;
        const float maxUnits=c.maxDistance/0.0254f;
        if(!n.position.Finite()||delta.Dot(delta)>maxUnits*maxUnits)continue;
        n.meters=delta.Length()*0.0254f;
        n.mins=Field<Vec3>(npc,profile::collision+0x20);n.maxs=Field<Vec3>(npc,profile::collision+0x2c);
        n.hp=Field<int>(npc,EntityFields(npc).health);n.maxHp=VCall<int>(npc,117);
        n.name=std::strncmp(name,"npc_",4)==0?name+4:name;
        // ESP boxes do not need server bone setup. Precise head hitboxes remain in BulletsHook.
        n.head=n.position+(n.mins+n.maxs)*0.5f;n.visible=true;
        if(c.espVisibility){
            auto& cached=visibilityCache[npc];cached.seen=now;
            n.visible=cached.visible;n.visibilityKnown=cached.known;targets.push_back(npc);
        }
        s.npcs.push_back(std::move(n));
    }
    if(c.espVisibility){
        for(auto it=visibilityCache.begin();it!=visibilityCache.end();)
            if(it->second.seen!=now)it=visibilityCache.erase(it);else ++it;
        const auto count=targets.size();unsigned traces=0;
        const auto start=count?visibilityCursor%count:0;
        // Round-robin prevents large NPC crowds from concentrating traces in one frame.
        for(std::size_t i=0;i<count&&traces<4;++i){
            const auto index=(start+i)%count;auto& cached=visibilityCache[targets[index]];
            if(cached.known&&now-cached.checked<200)continue;
            cached.visible=Visible(eye,s.npcs[index].head,player,targets[index]);
            cached.known=true;cached.checked=now;++traces;visibilityCursor=index+1;
        }
        for(std::size_t i=0;i<count;++i){
            const auto& cached=visibilityCache[targets[i]];
            s.npcs[i].visible=cached.visible;s.npcs[i].visibilityKnown=cached.known;
        }
    }else visibilityCache.clear();
    WriteSnapshot(std::move(s));
}
void Update(){UpdateAt(GetTickCount64());}
using FrameFn=void(__thiscall*)(void*,bool);
FrameFn originalFrame=nullptr;
void __fastcall FrameHook(void* self,void*,bool simulating){
    ++frameCalls;
    if(simulating&&ReadSettings().noRecoil)ClearPunch(LocalPlayer());
    originalFrame(self,simulating);
    if(simulating&&ReadSettings().noRecoil)ClearPunch(LocalPlayer());
    Update();
}
struct Bullets {int shots;Vec3 source,direction,spread;float range;int ammo,tracer;float damage;int playerDamage,flags;float force;void* attacker;void* ignore;bool primary;};
static_assert(sizeof(Bullets)==80);
using BulletsFn=void(__thiscall*)(void*,const Bullets&);
BulletsFn originalBullets=nullptr;
void __fastcall BulletsHook(void* self,void*,const Bullets& info){
    ++bulletCalls;
    static thread_local bool active=false;
    if(active){originalBullets(self,info);return;}
    struct Scope {bool& value;explicit Scope(bool& v):value(v){value=true;}~Scope(){value=false;}} scope(active);
    auto c=ReadSettings();void* player=LocalPlayer();
    if(player&&self==player)++playerShots;
    if(!player||self!=player||(!c.silentAim&&!c.noSpread&&!c.noRecoil)){originalBullets(self,info);return;}
    Bullets modified=info;
    if(c.noSpread)modified.spread={};
    Vec3 angles{};VCall<void>(engine,19,&angles);Vec3 forward=Forward(angles);
    if(c.noRecoil)modified.direction=forward;
    if(c.silentAim&&(c.activation==0||(GetAsyncKeyState(VK_RBUTTON)&0x8000))){
        float best=c.aimFov;Vec3 aim{};bool found=false;int visited=0;
        for(void* npc=VCall<void*>(tools,profile::toolsFirstEntity);npc&&visited++<8192;npc=VCall<void*>(tools,profile::toolsNextEntity,npc)){
            char name[128]{};if(!Enemy(npc,player,c,name))continue;
            Vec3 point;bool head;if(!AimPoint(npc,point,head))continue;
            Vec3 delta=point-info.source;float distance=delta.Length();if(distance<0.01f||distance>c.maxDistance/0.0254f||distance>info.range)continue;
            float angle=AngleBetween(forward,delta);if(angle>best)continue;
            if(!CanHitTarget(info.source,point,player,npc,info.ignore))continue;
            best=angle;aim=delta.Normalized();found=true;
        }
        if(found){modified.direction=aim;++aimedShots;}
    }
    originalBullets(self,modified);
}
struct RecvTable;
struct RecvProp{const char* name;int type,flags,stringSize;bool inArray;void* extra;RecvProp* array;void* lengthProxy;void* variableProxy;void* tableProxy;RecvTable* child;int offset,stride,count;const char* parent;};
struct RecvTable{RecvProp* props;int count;void* decoder;const char* name;};
struct ClientClass{void* create;void* event;const char* name;RecvTable* table;ClientClass* next;int id;};
static_assert(sizeof(RecvProp)==60);
int clientPunch=-1,clientPunchVelocity=-1;
std::optional<int> Netvar(RecvTable* t,const char* name,int base=0,int depth=0){
    if(depth>20||!Readable(t,sizeof(RecvTable))||t->count<0||t->count>2048||!Readable(t->props,t->count*sizeof(RecvProp)))return {};
    for(int i=0;i<t->count;++i){auto& p=t->props[i];if(p.offset<0||p.offset>0x20000)continue;if(StringIs(p.name,name))return base+p.offset;if(p.child){auto found=Netvar(p.child,name,base+p.offset,depth+1);if(found)return found;}}
    return {};
}
using StageFn=void(__thiscall*)(void*,int);
StageFn originalStage=nullptr;
using SnapFn=void(__thiscall*)(void*,const Vec3&);
SnapFn originalSnap=nullptr;
void __fastcall SnapHook(void* self,void*,const Vec3& angles){
    const auto caller=reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
    if(activeBuild&&caller==serverAddress+activeBuild->revolverRecoilReturn&&ReadSettings().noRecoil&&self==LocalPlayer())return;
    originalSnap(self,angles);
}
void __fastcall StageHook(void* self,void*,int stage){
    ++stageCalls;
    originalStage(self,stage);
    if(stage!=5||!ReadSettings().noRecoil||!VCall<bool>(engine,26)||VCall<int>(engine,21)!=1)return;
    void* local=VCall<void*>(entityList,3,VCall<int>(engine,12));if(!local)return;
    if(clientPunch>=0)Field<Vec3>(local,clientPunch)={};if(clientPunchVelocity>=0)Field<Vec3>(local,clientPunchVelocity)={};
}
}
bool InitializeGame(){
    if(initialized)return true;
    auto server=GetModuleHandleW(L"server.dll"),client=GetModuleHandleW(L"client.dll"),engineModule=GetModuleHandleW(L"engine.dll");
    if(!server||!client||!engineModule)return false;
    std::string serverError,clientError,engineError;
    const auto serverHash=FileHash(server,&serverError),clientHash=FileHash(client,&clientError),engineHash=FileHash(engineModule,&engineError);
    auto logModule=[](const char* name,HMODULE module,const std::string& hash,const std::string& error){Log(std::string(name)+": path="+Utf8(ModulePath(module))+", sha256="+hash+(error.empty()?"":"; "+error));};
    logModule("server.dll",server,serverHash,serverError);logModule("client.dll",client,clientHash,clientError);logModule("engine.dll",engineModule,engineHash,engineError);
    if(serverHash.empty()||clientHash.empty()||engineHash.empty()){
        const char* name=serverHash.empty()?"server.dll":clientHash.empty()?"client.dll":"engine.dll";
        SetStatus(std::string("Cannot read ")+name+" fingerprint / see UNJIWARE.log");Log(Status());return true;
    }
    activeBuild=profile::FindGameBuild(serverHash,clientHash);
    if(!activeBuild){SetStatus("Unsupported server/client DLL pair / see UNJIWARE.log");Log(Status());return true;}
    if(engineHash!=profile::engineHash){SetStatus("Unsupported engine.dll build / see UNJIWARE.log");Log(Status());return true;}
    Log(std::string("Selected profile: ")+activeBuild->name);
    engine=Interface(L"engine.dll","VEngineClient014");tools=Interface(L"server.dll","VSERVERTOOLS002");if(!tools)tools=Interface(L"server.dll","VSERVERTOOLS001");
    trace=Interface(L"engine.dll","EngineTraceServer003");entityList=Interface(L"client.dll","VClientEntityList003");
    void* clientDll=Interface(L"client.dll","VClient017");void* gameDll=Interface(L"server.dll","ServerGameDLL009");
    HMODULE inputModule=GetModuleHandleW(L"inputsystem.dll");
    if(inputModule&&FileHash(inputModule)==profile::inputHash)input=Interface(L"inputsystem.dll","InputSystemVersion001");
    auto physicsModule=GetModuleHandleW(L"vphysics.dll");
    if(physicsModule&&FileHash(physicsModule)==profile::physicsHash)
        physicsProps=Interface(L"vphysics.dll","VPhysicsSurfaceProps001");
    Log(physicsProps?"Bullet path: MASK_SHOT + verified glass surface interface":"Bullet path: MASK_SHOT; glass surface interface unavailable");
    if(!engine||!tools||!trace||!entityList||!clientDll||!gameDll){SetStatus("Required Source interface unavailable");Log(Status());return true;}
    const int toolSlots[]{profile::toolsGetPlayerPosition,profile::toolsFirstEntity,profile::toolsNextEntity,profile::toolsGetKeyValue,profile::toolsGetEntity};
    const std::uintptr_t toolRvas[]{activeBuild->toolsPosition,activeBuild->toolsFirst,activeBuild->toolsNext,activeBuild->toolsKeyValue,activeBuild->toolsEntity};
    for(int i=0;i<5;++i){
        auto address=reinterpret_cast<std::uintptr_t>((*reinterpret_cast<void***>(tools))[toolSlots[i]]);
        if(address-reinterpret_cast<std::uintptr_t>(server)!=toolRvas[i]){
            SetStatus("Server tools ABI validation failed");Log(Status());return true;
        }
    }
    auto* frame=(*reinterpret_cast<void***>(gameDll))[5];
    auto* bullets=reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(server)+activeBuild->fireBullets);
    if(reinterpret_cast<std::uintptr_t>(frame)-reinterpret_cast<std::uintptr_t>(server)!=activeBuild->gameFrame){SetStatus("GameFrame validation failed");Log(Status());return true;}
    ClientClass* cc=VCall<ClientClass*>(clientDll,8);
    for(int visited=0;cc&&visited++<2048&&Readable(cc,sizeof(ClientClass));cc=cc->next){
        if(!cc->table||!StringIs(cc->table->name,"DT_BasePlayer"))continue;
        clientPunch=Netvar(cc->table,"m_vecPunchAngle").value_or(Netvar(cc->table,"m_vecPunchAngle[0]").value_or(-1));
        clientPunchVelocity=Netvar(cc->table,"m_vecPunchAngleVel").value_or(Netvar(cc->table,"m_vecPunchAngleVel[0]").value_or(-1));break;
    }
    Log("Client netvars: punch="+std::to_string(clientPunch)+", velocity="+std::to_string(clientPunchVelocity));
    serverAddress=reinterpret_cast<std::uintptr_t>(server);
    auto* stage=(*reinterpret_cast<void***>(clientDll))[35];
    void* addresses[]{frame,bullets,stage,reinterpret_cast<void*>(serverAddress+activeBuild->snapEyeAngles)};
    void* callbacks[]{reinterpret_cast<void*>(&FrameHook),reinterpret_cast<void*>(&BulletsHook),reinterpret_cast<void*>(&StageHook),reinterpret_cast<void*>(&SnapHook)};
    void** originals[]{reinterpret_cast<void**>(&originalFrame),reinterpret_cast<void**>(&originalBullets),reinterpret_cast<void**>(&originalStage),reinterpret_cast<void**>(&originalSnap)};
    int installed=0;
    for(;installed<4;++installed)if(MH_CreateHook(addresses[installed],callbacks[installed],originals[installed])!=MH_OK)break;
    if(installed==4){for(int i=0;i<4;++i)if(MH_EnableHook(addresses[i])!=MH_OK){installed=-1;break;}}
    if(installed!=4){for(auto* a:addresses){MH_DisableHook(a);MH_RemoveHook(a);}SetStatus("Game hook installation failed");Log(Status());return true;}
    Log("R7: tools ABI validated: position=3, first=7, next=8, key=10, entity=20; projection=36");
    initialized=true;SetStatus(std::string(activeBuild->name)+" / ready (R7)");Log(Status());return true;
}
bool GameMatrix(Matrix4x4& matrix){
    if(!initialized||!engine||!VCall<bool>(engine,26))return false;
    const Matrix4x4* m=VCall<const Matrix4x4*>(engine,profile::engineWorldToScreen);if(!Readable(m,sizeof(matrix)))return false;matrix=*m;return true;
}
void GameHeartbeat(){
    if(!initialized)return;
    static ULONGLONG next=0;
    auto now=GetTickCount64();if(now<next)return;next=now+5000;
    const auto snapshot=ReadSnapshot();
    char line[512];
    std::snprintf(line,sizeof(line),"frames=%u, stages=%u, bullets=%u, player_shots=%u, aimed_shots=%u; %s; index=%d, clients=%d, NPCs=%zu; ESP=%.3f ms, peak=%.3f ms, updates=%u; LOS=%u, blocked=%u, invalid=%u",
        frameCalls.load(),stageCalls.load(),bulletCalls.load(),playerShots.load(),aimedShots.load(),
        GateName(gate.load()),localIndex.load(),maxClients.load(),snapshot.npcs.size(),espLastUs.load()/1000.0,espPeakUs.load()/1000.0,espUpdates.load(),visibilityChecks.load(),visibilityBlocked.load(),visibilityInvalid.load());
    SetDiagnostics(line);Log(std::string("Runtime: ")+line);
}
void CaptureGameInput(bool capture){
    static bool last=false,applied=false;
    void* system=input.load();
    if(system&&(!applied||last!=capture)){VCall<void>(system,25);VCall<void>(system,7,!capture);last=capture;applied=true;}
}
}
