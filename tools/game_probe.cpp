// Exercises the production game paths through x86 ABI-shaped test doubles.
// The installed game is never opened or modified by this executable.
#include "../src/game.cpp"
#include <array>
#include <cstdio>
#include <cstdlib>
namespace {
int failed=0,clients=1,index=1,relation=1;bool inGame=true;
const char* testNpcClass="npc_combine_s";
float targetY=5,traceFraction=1;
bool traceStartSolid=false,traceAllSolid=false,traceHitsTarget=false;
enum class Scene {Plain,Grate,Glass,GlassWall,ThickGlass,MissingExit,TwoGlass};
Scene scene=Scene::Plain;
bool recurseShot=false,recursing=false;
const char* glassClass="func_breakable";
unsigned short glassMaterial='Y';
unsigned firstCalls=0,boneCalls=0,traceCalls=0;
bool crowdMode=false;
alignas(16) std::array<std::array<unsigned char,0x2000>,9> crowd{};
void Check(bool ok,const char* message){if(!ok){++failed;std::printf("FAIL: %s\n",message);}}
alignas(16) std::array<unsigned char,0x2000> player{},npc{},glass{},additional{};
std::array<void*,144> engineTable{};std::array<void*,33> toolsTable{};
std::array<void*,464> playerTable{},npcTable{};
std::array<void*,5> traceTable{},entityTable{};
std::array<void*,6> physicsTable{};
struct Object{void** table;};
Object fakeEngine{engineTable.data()},fakeTools{toolsTable.data()},fakeTrace{traceTable.data()},fakeEntities{entityTable.data()},fakePhysics{physicsTable.data()};
uw::Matrix4x4 projection{};
std::array<unsigned char,512> studio{};
void* studioPointer=studio.data();
uw::DataField fields[4]{};
uw::DataField glassField{};
uw::DataMap glassMap{&glassField,1,"CBreakable",nullptr};
std::array<unsigned char,96> surface{};
uw::DataMap playerMap{fields,4,"CHL2_Player",nullptr},npcMap{fields,4,"CAI_BaseNPC",nullptr};
uw::Bullets captured{};int originalShots=0;
bool __fastcall InGame(void*,void*){return inGame;}
int __fastcall Clients(void*,void*){return clients;}
int __fastcall Index(void*,void*){return index;}
void __fastcall Angles(void*,void*,uw::Vec3* v){*v={};}
const uw::Matrix4x4* __fastcall Matrix(void*,void*){return &projection;}
void* __fastcall Entity(void*,void*,int n){Check(n==1,"entity index");return player.data();}
void* __fastcall First(void*,void*){++firstCalls;return crowdMode?crowd[0].data():npc.data();}
void* __fastcall Next(void*,void*,void* n){
    if(!crowdMode){Check(n==npc.data(),"entity traversal");return nullptr;}
    for(std::size_t i=0;i<crowd.size();++i)if(n==crowd[i].data())return i+1<crowd.size()?crowd[i+1].data():nullptr;
    Check(false,"crowd traversal");return nullptr;
}
bool __fastcall Position(void*,void*,uw::Vec3* eye,uw::Vec3* angle,void*){*eye={0,0,64};*angle={};return true;}
int __fastcall Fov(void*,void*,void*){return 75;}
bool __fastcall KeyValue(void*,void*,void* n,const char* key,char* out,int length){
    Check((crowdMode||n==npc.data()||n==glass.data())&&std::strcmp(key,"classname")==0&&length>=12,"key value ABI");
    std::strcpy(out,n==glass.data()?glassClass:testNpcClass);return true;
}
uw::DataMap* __fastcall Map(void* self,void*){return self==glass.data()?&glassMap:self==player.data()?&playerMap:&npcMap;}
int __fastcall Relation(void*,void*,void* p){Check(p==player.data(),"relationship target");return relation;}
int __fastcall Health(void*,void*){return 100;}
void __fastcall Bone(void*,void*,int n,uw::Matrix3x4* matrix){
    ++boneCalls;Check(n==0,"head bone");*matrix={};matrix->m[0][0]=matrix->m[1][1]=matrix->m[2][2]=1;
    matrix->m[0][3]=100;matrix->m[1][3]=targetY;matrix->m[2][3]=64;
}
void __fastcall Trace(void*,void*,const uw::Ray* ray,unsigned mask,uw::TraceFilter* filter,uw::Trace* out){
    const auto* raw=reinterpret_cast<const unsigned char*>(ray);
    Check(raw[0x40]==1&&raw[0x41]==1,"pinned HL2 engine sees ray and sweep flags at raw offsets 0x40/0x41");
    ++traceCalls;Check(!filter->ShouldHit(player.data(),mask),"trace skips local player");
    Check(mask==0x46004003,"trace mask matches installed FireBullets and excludes CONTENTS_GRATE");
    if(scene!=Scene::Plain){
        uw::Vec3 start{ray->start[0],ray->start[1],ray->start[2]},delta{ray->delta[0],ray->delta[1],ray->delta[2]};
        *out={};out->start=start;out->end=start+delta;out->fraction=1;
        if(scene==Scene::Grate&&!(mask&8))return;
        if(scene==Scene::MissingExit&&delta.x<0)return;
        struct Slab{float low,high;bool isGlass;};
        Slab slabs[2]{{40,scene==Scene::ThickGlass?64.0f:44.0f,true},{70,80,false}};int count=1;
        if(scene==Scene::Grate)slabs[0].isGlass=false;
        if(scene==Scene::GlassWall)count=2;
        if(scene==Scene::TwoGlass){slabs[0]={20,24,true};slabs[1]={60,64,true};count=2;}
        for(int i=0;i<count;++i){
            const auto slab=slabs[i];
            if(start.x>slab.low+0.001f&&start.x<slab.high-0.001f){out->startSolid=true;out->fraction=0;out->end=start;return;}
            if(std::abs(delta.x)<0.00001f)continue;
            const float at=((delta.x>0?slab.low:slab.high)-start.x)/delta.x;
            if(at<0||at>=out->fraction)continue;
            out->fraction=at;out->end=start+delta*at;out->entity=slab.isGlass?glass.data():nullptr;out->surfaceProps=0;
        }return;
    }
    *out={};out->fraction=traceFraction;out->startSolid=traceStartSolid;out->allSolid=traceAllSolid;out->entity=traceHitsTarget?npc.data():nullptr;
}
int __fastcall SurfaceCount(void*,void*){return 1;}
void* __fastcall SurfaceData(void*,void*,int index){Check(index==0,"surface index");uw::Field<unsigned short>(surface.data(),0x48)=glassMaterial;return surface.data();}
void __fastcall OriginalBullets(void* self,void*,const uw::Bullets& info){
    captured=info;++originalShots;
    if(recurseShot&&!recursing){recursing=true;auto inner=info;inner.direction={0,1,0};uw::BulletsHook(self,nullptr,inner);recursing=false;}
}
void __fastcall OriginalFrame(void*,void*,bool){}
void __fastcall OriginalStage(void*,void*,int){}
template<class T>void Put(void** table,int slot,T function){table[slot]=reinterpret_cast<void*>(function);}
bool Near(uw::Vec3 a,uw::Vec3 b){return (a-b).Length()<0.0001f;}
}
int main(){
    CreateDirectoryW(L"probe-logs",nullptr);uw::SetDirectory(L".\\probe-logs");
    const auto ini=uw::OutputPath(L"UNJIWARE.ini");
    WritePrivateProfileStringW(L"settings",L"visibleOnly",L"0",ini.c_str());WritePrivateProfileStringW(L"settings",L"espVisibility",L"0",ini.c_str());uw::LoadSettings();
    Check(uw::ReadSettings().visibleOnly&&uw::ReadSettings().espVisibility,"legacy INI cannot disable wall check or colors");
    Put(engineTable.data(),26,InGame);Put(engineTable.data(),21,Clients);Put(engineTable.data(),12,Index);
    Put(engineTable.data(),19,Angles);Put(engineTable.data(),36,Matrix);
    Put(toolsTable.data(),20,Entity);Put(toolsTable.data(),7,First);Put(toolsTable.data(),8,Next);
    Put(toolsTable.data(),10,KeyValue);Put(toolsTable.data(),3,Position);Put(toolsTable.data(),5,Fov);
    Put(playerTable.data(),11,Map);Put(npcTable.data(),11,Map);
    Put(npcTable.data(),296,Relation);Put(npcTable.data(),117,Health);Put(npcTable.data(),198,Bone);
    Put(physicsTable.data(),2,SurfaceCount);Put(physicsTable.data(),5,SurfaceData);
    glassField.name="m_spawnflags";glassField.offset[0]=0xfc;
    uw::Field<void*>(glass.data(),0)=npcTable.data();
    Put(traceTable.data(),4,Trace);Put(entityTable.data(),3,Entity);
    uw::Field<void*>(player.data(),0)=playerTable.data();uw::Field<void*>(npc.data(),0)=npcTable.data();
    const char* names[]{"m_iHealth","m_lifeState","m_vecPunchAngle","m_vecPunchAngleVel"};
    int offsets[]{0xe0,0xe4,0x8cc,0x8d8};
    for(int i=0;i<4;++i){fields[i].name=names[i];fields[i].offset[0]=offsets[i];}
    uw::Field<int>(player.data(),0xe0)=100;uw::Field<int>(npc.data(),0xe0)=90;
    uw::Field<uw::Vec3>(npc.data(),0x27c)={100,5,0};
    uw::Field<uw::Vec3>(npc.data(),0x168)={-16,-16,0};uw::Field<uw::Vec3>(npc.data(),0x174)={16,16,72};
    uw::Field<void*>(npc.data(),0x44c)=&studioPointer;
    uw::Field<int>(studio.data(),0)=0x54534449;uw::Field<int>(studio.data(),76)=512;
    uw::Field<int>(studio.data(),156)=1;uw::Field<int>(studio.data(),172)=1;uw::Field<int>(studio.data(),176)=180;
    uw::Field<int>(studio.data(),184)=1;uw::Field<int>(studio.data(),188)=12;
    uw::Field<int>(studio.data(),196)=1;
    uw::Field<uw::Vec3>(studio.data(),200)={-2,-2,-2};uw::Field<uw::Vec3>(studio.data(),212)={2,2,2};
    uw::engine=&fakeEngine;uw::tools=&fakeTools;uw::trace=&fakeTrace;uw::entityList=&fakeEntities;
    uw::originalBullets=reinterpret_cast<uw::BulletsFn>(OriginalBullets);
    uw::originalFrame=reinterpret_cast<uw::FrameFn>(OriginalFrame);
    uw::originalStage=reinterpret_cast<uw::StageFn>(OriginalStage);
    Check(uw::LocalPlayer()==player.data(),"server player lookup");
    inGame=false;Check(!uw::LocalPlayer(),"out of game gate");inGame=true;
    clients=2;Check(!uw::LocalPlayer(),"singleplayer gate");clients=1;
    index=0;Check(!uw::LocalPlayer(),"invalid local index gate");index=1;
    uw::Settings settings;settings.esp=true;uw::WriteSettings(settings);
    uw::Update();auto snapshot=uw::ReadSnapshot();
    Check(snapshot.inGame&&snapshot.npcs.size()==1&&snapshot.viewFov==75,"NPC snapshot through real production update");
    if(!snapshot.npcs.empty())Check(!snapshot.npcs[0].headHitbox&&snapshot.npcs[0].visible&&snapshot.npcs[0].visibilityKnown&&snapshot.npcs[0].hp==90,"ESP uses bounds without bone setup");
    uw::Bullets input{};input.shots=1;input.source={0,0,64};input.direction={1,0.2f,0};input.spread={0.1f,0.1f,0.1f};input.range=8192;
    settings.noSpread=true;uw::WriteSettings(settings);uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.spread,{})&&Near(captured.direction,input.direction),"no spread changes only spread");
    uw::BulletsHook(npc.data(),nullptr,input);Check(Near(captured.spread,input.spread),"NPC shots unchanged");
    settings.silentAim=true;settings.visibleOnly=true;uw::WriteSettings(settings);uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,uw::Vec3{100,5,0}.Normalized()),"silent head aim with visibility trace");
    settings.visibleOnly=false;uw::WriteSettings(settings);
    Check(uw::ReadSettings().visibleOnly,"wall check enforced after settings write");
    const auto aimedBeforeWall=uw::aimedShots.load();
    Check(uw::visibilityCache[npc.data()].visible,"ESP has a cached clear result before wall test");
    traceFraction=0.5f;uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,input.direction)&&uw::aimedShots.load()==aimedBeforeWall,"blocked head rejected despite cached ESP visibility and legacy setting");
    uw::UpdateAt(GetTickCount64()+333);
    const auto blockedSnapshot=uw::ReadSnapshot();
    Check(blockedSnapshot.npcs.size()==1&&blockedSnapshot.npcs[0].visibilityKnown&&!blockedSnapshot.npcs[0].visible,"blocked ray is published as blocked ESP color");
    traceFraction=0.995f;uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,input.direction),"thin wall immediately before head rejected");
    traceFraction=1;traceStartSolid=true;uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,input.direction),"trace starting inside solid rejected");traceStartSolid=false;
    traceAllSolid=true;uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,input.direction),"fully solid trace rejected");traceAllSolid=false;
    traceFraction=NAN;uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,input.direction),"invalid trace result rejected");
    traceFraction=0.8f;traceHitsTarget=true;uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,uw::Vec3{100,5,0}.Normalized()),"direct hit on target accepted");
    traceFraction=1;traceHitsTarget=false;
        relation=3;uw::BulletsHook(player.data(),nullptr,input);Check(Near(captured.direction,input.direction),"friendly NPC excluded");relation=1;
    targetY=50;uw::BulletsHook(player.data(),nullptr,input);Check(Near(captured.direction,input.direction),"outside FOV excluded");targetY=5;
    settings.noRecoil=true;uw::WriteSettings(settings);
    uw::Field<uw::Vec3>(player.data(),0x8cc)={1,2,3};uw::Field<uw::Vec3>(player.data(),0x8d8)={1,2,3};
    uw::FrameHook(nullptr,nullptr,true);
    Check(Near(uw::Field<uw::Vec3>(player.data(),0x8cc),{})&&Near(uw::Field<uw::Vec3>(player.data(),0x8d8),{}),"server recoil suppressed");
    uw::clientPunch=0xeb8;uw::clientPunchVelocity=0xef4;
    uw::Field<uw::Vec3>(player.data(),0xeb8)={1,2,3};uw::Field<uw::Vec3>(player.data(),0xef4)={1,2,3};
    uw::StageHook(nullptr,nullptr,5);
    Check(Near(uw::Field<uw::Vec3>(player.data(),0xeb8),{})&&Near(uw::Field<uw::Vec3>(player.data(),0xef4),{}),"client recoil suppressed");
    projection.m[0][0]=1.25f;uw::initialized=true;uw::Matrix4x4 result{};
    Check(uw::GameMatrix(result)&&result.m[0][0]==1.25f,"world to screen uses slot 36");
    uw::GameHeartbeat();Check(uw::Diagnostics().find("player found")!=std::string::npos,"runtime diagnostics");
    std::printf("Production-path checks: shots=%d, aimed=%u, NPCs=%zu\n",originalShots,uw::aimedShots.load(),snapshot.npcs.size());
    settings.noRecoil=false;settings.silentAim=true;uw::WriteSettings(settings);
    ULONGLONG filterTime=GetTickCount64()+1000;
    for(const auto* name:{"npc_pigeon","npc_crow","npc_seagull","npc_enemyfinder","npc_enemyfinder_combinecannon","npc_bullseye"}){
        testNpcClass=name;uw::UpdateAt(filterTime);filterTime+=33;
        Check(uw::ReadSnapshot().npcs.empty(),"wildlife and script entities excluded from ESP");
        uw::BulletsHook(player.data(),nullptr,input);
        Check(Near(captured.direction,input.direction),"wildlife and script entities excluded from aim");
    }
    testNpcClass="npc_combine_s";
        uw::physicsProps=&fakePhysics;settings.silentAim=true;settings.noRecoil=false;uw::WriteSettings(settings);
    scene=Scene::Grate;uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,uw::Vec3{100,5,0}.Normalized()),"bullets can target through a grate");
    scene=Scene::Glass;uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,uw::Vec3{100,5,0}.Normalized()),"permitted thin breakable glass is penetrable");
    scene=Scene::GlassWall;uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,input.direction),"solid wall behind glass still blocks target");
    scene=Scene::ThickGlass;uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,input.direction),"glass thicker than engine limit rejected");
    scene=Scene::MissingExit;uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,input.direction),"glass with no verified exit rejected");
    scene=Scene::Glass;uw::Field<int>(glass.data(),0xfc)=0x0800;uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,input.direction),"no-bullet-penetration glass flag respected");uw::Field<int>(glass.data(),0xfc)=0;
    glassMaterial='C';uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,input.direction),"non-glass material blocks target");glassMaterial='Y';
    glassClass="prop_physics";uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,input.direction),"glass prop without engine penetration behavior rejected");glassClass="func_breakable";
    scene=Scene::TwoGlass;uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,uw::Vec3{100,5,0}.Normalized()),"two separated glass panes handled");
    uw::physicsProps=nullptr;scene=Scene::Glass;uw::BulletsHook(player.data(),nullptr,input);
    Check(Near(captured.direction,input.direction),"missing surface interface does not permit glass blindly");
    scene=Scene::Plain;
    Check(!uw::CanHitTarget(input.source,{100,5,64},player.data(),npc.data(),npc.data()),"additional ignored entity cannot be selected");
    recurseShot=true;uw::BulletsHook(player.data(),nullptr,input);recurseShot=false;
    Check(Near(captured.direction,{0,1,0}),"engine glass continuation is not re-aimed recursively");
        settings.silentAim=false;settings.noRecoil=false;settings.espVisibility=false;uw::WriteSettings(settings);
    uw::nextEspUpdate=0;uw::visibilityCache.clear();
    ULONGLONG time=GetTickCount64()+1000;
    auto scans=firstCalls,bones=boneCalls,traces=traceCalls;
    for(unsigned i=0;i<1000;++i)uw::UpdateAt(time+i);
    Check(firstCalls-scans==31,"1000 frame attempts perform only 31 ESP scans");
    Check(boneCalls==bones,"ESP performs zero bone transforms");
    Check(traceCalls-traces==5,"mandatory wall colors perform only five traces per second for one NPC");
    std::printf("ESP budget: attempts=1000, scans=%u, bone_calls=%u, traces=%u\n",firstCalls-scans,boneCalls-bones,traceCalls-traces);
    settings.espVisibility=true;uw::WriteSettings(settings);traces=traceCalls;time+=2000;
    uw::UpdateAt(time);Check(traceCalls==traces+1,"optional visibility first refresh");
    uw::UpdateAt(time+33);Check(traceCalls==traces+1,"visibility result reused within 200 ms");
    uw::UpdateAt(time+200);Check(traceCalls==traces+2,"visibility refreshed after 200 ms");
    for(auto& entity:crowd)entity=npc;
    crowdMode=true;uw::visibilityCache.clear();uw::visibilityCursor=0;time+=1000;traces=traceCalls;
    uw::UpdateAt(time);Check(traceCalls-traces==4,"visibility budget limited to four traces");
    uw::UpdateAt(time+33);Check(traceCalls-traces==8,"round robin continues next NPCs");
    uw::UpdateAt(time+66);Check(traceCalls-traces==9,"round robin covers remaining NPC");
    Check(uw::visibilityCache.size()==9,"crowd visibility cache populated");
    crowdMode=false;uw::UpdateAt(time+99);Check(uw::visibilityCache.size()==1,"removed NPC cache entries pruned");
    settings.esp=false;uw::WriteSettings(settings);uw::UpdateAt(time+100);
    Check(uw::ReadSnapshot().npcs.empty()&&uw::visibilityCache.empty(),"disabling ESP clears display and visibility cache immediately");
        std::puts("Ray ABI, wall blocking, grate/glass reachability, recursion, and NPC exclusions verified.");
    std::puts(failed?"GAME PROBE FAILED":"GAME PROBE PASSED");return failed?1:0;
}
