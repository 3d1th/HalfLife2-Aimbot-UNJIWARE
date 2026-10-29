#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "ui.hpp"
#include <cstdio>

namespace uw {
namespace {
ImFont* titleFont=nullptr;
int page=0;
constexpr ImVec4 panel{0.012f,0.059f,0.087f,1.0f};
void Toggle(const char* label,bool& value,const Settings& cfg){
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(label);
    const float h=ImGui::GetFrameHeight();
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x-28);
    ImVec2 at=ImGui::GetCursorScreenPos();
    if(ImGui::InvisibleButton("toggle",{28,h}))value=!value;
    auto* d=ImGui::GetWindowDrawList();float y=at.y+h/2;
    d->AddRectFilled({at.x,y-5},{at.x+28,y+5},value?IM_COL32(0,60,80,255):IM_COL32(24,34,42,255),5);
    ImU32 color=value?ImGui::ColorConvertFloat4ToU32({cfg.accent[0],cfg.accent[1],cfg.accent[2],1}):IM_COL32(91,111,124,255);
    d->AddCircleFilled({at.x+(value?22:6),y},5,color);
    ImGui::PopID();
}
void Slider(const char* name,float& v,float low,float high,const char* format){
    ImGui::PushID(name);ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(name);
    float w=ImGui::GetWindowWidth()*0.49f;
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x-w);ImGui::SetNextItemWidth(w);
    ImVec2 p=ImGui::GetCursorScreenPos();float h=ImGui::GetFrameHeight(),track=w-42;
    ImGui::InvisibleButton("##value",{w,h});
    if(ImGui::IsItemActive()&&ImGui::IsMouseDown(ImGuiMouseButton_Left))v=low+(high-low)*std::clamp((ImGui::GetIO().MousePos.x-p.x)/track,0.0f,1.0f);
    if(ImGui::IsItemFocused()){
        float step=(high-low)/100;
        if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow))v=std::max(low,v-step);
        if(ImGui::IsKeyPressed(ImGuiKey_RightArrow))v=std::min(high,v+step);
    }
    auto* d=ImGui::GetWindowDrawList();float y=p.y+h/2,t=(v-low)/(high-low);
    ImU32 cyan=ImGui::GetColorU32(ImGuiCol_SliderGrab);
    d->AddLine({p.x,y},{p.x+track,y},IM_COL32(16,55,73,255),1.5f);
    d->AddLine({p.x,y},{p.x+track*t,y},cyan,1.5f);d->AddCircleFilled({p.x+track*t,y},4,cyan);
    char text[32];std::snprintf(text,sizeof(text),format,v);auto size=ImGui::CalcTextSize(text);
    d->AddRectFilled({p.x+track+8,p.y+3},{p.x+w,p.y+h-3},IM_COL32(2,15,23,255),1);
    d->AddText({p.x+w-size.x-3,y-size.y/2},IM_COL32(205,227,242,255),text);ImGui::PopID();
}
void BeginCard(const char* id,const char* title,float height){
    ImGui::PushStyleColor(ImGuiCol_ChildBg,panel);
    ImGui::BeginChild(id,{0,height},false,ImGuiWindowFlags_AlwaysUseWindowPadding);
    ImGui::TextUnformatted(title);ImGui::Separator();ImGui::Spacing();
}
void EndCard(){ImGui::EndChild();ImGui::PopStyleColor();}
void Filters(Settings& c,float height){
    BeginCard("filters","Enemy NPCs",height);
    Toggle("Combine",c.combine,c);Toggle("Zombies",c.zombies,c);
    Toggle("Antlions",c.antlions,c);Toggle("Headcrabs",c.headcrabs,c);
    Toggle("Other hostile NPCs",c.other,c);EndCard();
}
void Nav(const char* text,int index,const Settings& c){
    ImVec2 start=ImGui::GetCursorScreenPos();
    if(ImGui::Selectable(text,page==index,0,{156,29}))page=index;
    auto* d=ImGui::GetWindowDrawList();ImU32 color=ImGui::ColorConvertFloat4ToU32({c.accent[0],c.accent[1],c.accent[2],1});
    ImVec2 center{start.x+12,start.y+14};
    if(index==0){d->AddCircle(center,4,color,12,1.3f);d->AddLine({center.x-7,center.y},{center.x+7,center.y},color);d->AddLine({center.x,center.y-7},{center.x,center.y+7},color);}
    else if(index==1){d->AddRect({center.x-5,center.y-5},{center.x+5,center.y+5},color);d->AddCircleFilled(center,1.5f,color);}
    else if(index==2){d->AddLine({center.x-6,center.y-3},{center.x+7,center.y-3},color,3);d->AddLine({center.x-2,center.y-2},{center.x-4,center.y+5},color,3);}
    else {d->AddCircle(center,5,color,6,1.5f);d->AddCircleFilled(center,2,color);}
}
void Watermark(const Settings& c,bool preview){
    if(!c.watermark)return;
    SYSTEMTIME t{};GetLocalTime(&t);char text[96];
    std::snprintf(text,sizeof(text),"UNJIWARE For HL2 | %02d:%02d:%02d",t.wHour,t.wMinute,t.wSecond);
    auto* d=ImGui::GetForegroundDrawList();ImVec2 p{12,12},s=ImGui::CalcTextSize(text);
    d->AddRectFilled(p,{p.x+s.x+24,p.y+28},IM_COL32(3,5,8,250),2);
    d->AddRectFilled(p,{p.x+2,p.y+28},ImGui::ColorConvertFloat4ToU32({c.accent[0],c.accent[1],c.accent[2],1}));
    d->AddText({p.x+12,p.y+(28-s.y)/2},IM_COL32(232,242,250,255),text);
    if(preview)d->AddText({12,48},IM_COL32(86,113,130,255),"UI PREVIEW / GAME FEATURES NOT ATTACHED");
}
}
void InitializeUiFonts(){
    wchar_t directory[MAX_PATH]{};GetWindowsDirectoryW(directory,MAX_PATH);
    char font[MAX_PATH]{};WideCharToMultiByte(CP_UTF8,0,directory,-1,font,MAX_PATH,nullptr,nullptr);
    std::string base=std::string(font)+"/Fonts/";
    auto& io=ImGui::GetIO();ImFontConfig fc{};fc.OversampleH=2;fc.OversampleV=2;
    if(!io.Fonts->AddFontFromFileTTF((base+"segoeui.ttf").c_str(),14,&fc))io.Fonts->AddFontDefault();
    titleFont=io.Fonts->AddFontFromFileTTF((base+"segoeuib.ttf").c_str(),22,&fc);
}
void DrawUi(bool& open,bool preview){
    Settings c=ReadSettings();Watermark(c,preview);if(!open)return;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,2);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,2);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{6,3});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{10,7});
    struct ColorPair{ImGuiCol id;ImVec4 color;};
    ColorPair colors[]{
        {ImGuiCol_WindowBg,{0.025f,0.032f,0.044f,1}},
        {ImGuiCol_Text,{0.83f,0.91f,0.96f,1}},
        {ImGuiCol_TextDisabled,{0.31f,0.42f,0.49f,1}},
        {ImGuiCol_Border,{0.10f,0.20f,0.26f,1}},
        {ImGuiCol_Separator,{0.035f,0.15f,0.21f,1}},
        {ImGuiCol_FrameBg,{0.018f,0.083f,0.12f,1}},
        {ImGuiCol_FrameBgHovered,{0.025f,0.14f,0.19f,1}},
        {ImGuiCol_Header,{0.025f,0.23f,0.32f,1}},
        {ImGuiCol_HeaderHovered,{0.035f,0.28f,0.38f,1}},
        {ImGuiCol_HeaderActive,{0.04f,0.32f,0.43f,1}},
        {ImGuiCol_Button,{0.018f,0.078f,0.11f,1}},
        {ImGuiCol_ButtonHovered,{0.025f,0.20f,0.28f,1}},
        {ImGuiCol_ButtonActive,{0.03f,0.28f,0.38f,1}},
        {ImGuiCol_SliderGrab,{c.accent[0],c.accent[1],c.accent[2],1}},
        {ImGuiCol_SliderGrabActive,{0.32f,0.74f,1,1}},
        {ImGuiCol_CheckMark,{c.accent[0],c.accent[1],c.accent[2],1}}
    };
    for(auto& p:colors)ImGui::PushStyleColor(p.id,p.color);
    ImGui::SetNextWindowPos({80,90},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({760,460},ImGuiCond_Always);
    if(ImGui::Begin("UNJIWARE",&open,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollbar)){
        ImGui::PushStyleColor(ImGuiCol_ChildBg,{0.09f,0.135f,0.17f,1});
        ImGui::BeginChild("sidebar",{176,0},false);
        ImGui::SetCursorPos({14,16});if(titleFont)ImGui::PushFont(titleFont);ImGui::TextColored({0.96f,0.98f,1,1},"UNJIWARE");if(titleFont)ImGui::PopFont();
        ImGui::SetCursorPos({16,46});ImGui::TextDisabled("HALF-LIFE 2");
        ImGui::SetCursorPos({10,83});ImGui::BeginGroup();
        ImGui::TextDisabled("Aimbot");Nav("          Silent Aim",0,c);ImGui::Spacing();
        ImGui::TextDisabled("Visuals");Nav("          Enemy ESP",1,c);ImGui::Spacing();
        ImGui::TextDisabled("Weapons");Nav("          Weapon Control",2,c);ImGui::Spacing();
        ImGui::TextDisabled("Miscellaneous");Nav("          Settings",3,c);ImGui::EndGroup();
        ImGui::SetCursorPos({16,429});ImGui::TextDisabled("INSERT  Toggle menu");
        ImGui::EndChild();ImGui::PopStyleColor();ImGui::SameLine(0,0);
        ImGui::BeginChild("right",{0,0},false);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{12,10});
        ImGui::BeginChild("topbar",{0,48},false,ImGuiWindowFlags_AlwaysUseWindowPadding);
        static std::uint64_t savedUntil=0;
        if(ImGui::Button("Save",{70,26})){WriteSettings(c);if(SaveSettings())savedUntil=GetTickCount64()+1600;}
        ImGui::SameLine();ImGui::SetNextItemWidth(152);
        if(ImGui::BeginCombo("##profile","Global")){if(ImGui::Selectable("Global",true)){}ImGui::EndCombo();}
        ImGui::SameLine();if(GetTickCount64()<savedUntil)ImGui::TextDisabled("Saved");
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x-100);ImGui::TextDisabled("HL2 / x86");
        ImGui::EndChild();ImGui::Separator();
        ImGui::SetCursorPosX(12);ImGui::BeginChild("body",{-12,350},false);
        if(ImGui::BeginTable("cards",2,ImGuiTableFlags_SizingStretchSame)){
            ImGui::TableNextColumn();
            if(page==0){
                BeginCard("aim-main","Main",202);Toggle("Enable Silent Aim",c.silentAim,c);
                ImGui::TextUnformatted("Target");ImGui::SameLine(140);ImGui::TextDisabled("Head hitbox");
                ImGui::TextUnformatted("Activation");ImGui::SameLine(118);ImGui::SetNextItemWidth(-1);
                ImGui::Combo("##activation",&c.activation,"On attack\0Hold right mouse\0");
                ImGui::TextUnformatted("Wall check");ImGui::SameLine(140);ImGui::TextDisabled("Always on");Toggle("Draw FOV",c.drawFov,c);EndCard();
                BeginCard("recoil","Recoil Control",128);Toggle("No Recoil",c.noRecoil,c);
                ImGui::TextDisabled("Suppress weapon kick");EndCard();
            }else if(page==1){
                BeginCard("esp-main","ESP",220);Toggle("Enable ESP",c.esp,c);Toggle("Bounding boxes",c.boxes,c);
                Toggle("NPC names",c.names,c);Toggle("Health bars",c.health,c);Toggle("Distance",c.distance,c);ImGui::TextUnformatted("Wall colors");ImGui::SameLine(140);ImGui::TextDisabled("Always on");EndCard();
                BeginCard("range","Range",110);Slider("Distance (m)",c.maxDistance,5,500,"%.0f");EndCard();
            }else if(page==2){
                BeginCard("weapon","Weapon Control",202);Toggle("No Spread",c.noSpread,c);Toggle("No Recoil",c.noRecoil,c);
                ImGui::Spacing();ImGui::TextDisabled("Hitscan weapons");EndCard();
                BeginCard("weapon-note","Recoil",128);ImGui::TextWrapped("Suppresses weapon kick, including the .357's view-angle kick.");EndCard();
            }else{
                BeginCard("display","Display",202);Toggle("Watermark",c.watermark,c);
                ImGui::ColorEdit3("Accent",c.accent.data(),ImGuiColorEditFlags_NoInputs);
                ImGui::Spacing();ImGui::TextDisabled("INSERT  Open / close menu");EndCard();
                BeginCard("config","Configuration",128);ImGui::TextWrapped("Save writes UNJIWARE.ini beside the DLL. Settings load at injection.");EndCard();
            }
            ImGui::TableNextColumn();
            if(page==0){BeginCard("aim-fov","Aim",113);Slider("FOV",c.aimFov,1,45,"%.1f");ImGui::TextDisabled("Camera remains unchanged");EndCard();Filters(c,217);}
            else if(page==3){Filters(c,175);BeginCard("runtime","Runtime diagnostics",155);ImGui::TextWrapped("%s",Diagnostics().c_str());EndCard();}
            else if(page==1){Filters(c,202);BeginCard("wall-colors","Wall visibility",128);ImGui::TextWrapped("Cyan: clear. Orange: blocked. Gray: pending. Aim checks the head on each shot.");EndCard();}
            else{Filters(c,202);BeginCard("scope","Target scope",128);ImGui::TextWrapped("Hostile NPCs only. Current NPC relationships exclude allies, including friendly antlions.");EndCard();}
            ImGui::EndTable();
        }
        ImGui::EndChild();ImGui::SetCursorPosX(16);ImGui::TextDisabled("%s",preview?"UI preview / no game attached":Status().c_str());
        ImGui::PopStyleVar();ImGui::EndChild();
    }
    ImGui::End();ImGui::PopStyleColor(IM_ARRAYSIZE(colors));ImGui::PopStyleVar(6);WriteSettings(c);
}
void DrawFov(float width,float height,const Matrix4x4* matrix){
    const auto c=ReadSettings();if(!c.drawFov||width<=0||height<=0)return;
    float scale=1.0f;
    if(matrix)scale=Vec3{matrix->m[0][0],matrix->m[0][1],matrix->m[0][2]}.Length();
    const float radius=std::tan(c.aimFov*0.01745329252f)*width*0.5f*scale;
    if(!std::isfinite(radius)||radius<=0||radius>width*2)return;
    ImGui::GetBackgroundDrawList()->AddCircle({width/2,height/2},radius,
        ImGui::ColorConvertFloat4ToU32({c.accent[0],c.accent[1],c.accent[2],0.7f}),128,1.25f);
}
void DrawEsp(const Matrix4x4& matrix,float width,float height){
    auto c=ReadSettings();auto s=ReadSnapshot();
    auto* d=ImGui::GetBackgroundDrawList();
    auto project=[&](Vec3 v,ImVec2& p){
        float w=matrix.m[3][0]*v.x+matrix.m[3][1]*v.y+matrix.m[3][2]*v.z+matrix.m[3][3];
        if(w<0.01f)return false;
        p={width/2*(1+(matrix.m[0][0]*v.x+matrix.m[0][1]*v.y+matrix.m[0][2]*v.z+matrix.m[0][3])/w),
           height/2*(1-(matrix.m[1][0]*v.x+matrix.m[1][1]*v.y+matrix.m[1][2]*v.z+matrix.m[1][3])/w)};
        return std::isfinite(p.x)&&std::isfinite(p.y);
    };
    if(!c.esp||!s.inGame||GetTickCount64()-s.updatedAt>250)return;
    for(auto& n:s.npcs){
        ImVec2 low{1e8f,1e8f},high{-1e8f,-1e8f};bool valid=true;
        for(int i=0;i<8;++i){
            Vec3 corner=n.position+Vec3{i&1?n.maxs.x:n.mins.x,i&2?n.maxs.y:n.mins.y,i&4?n.maxs.z:n.mins.z};
            ImVec2 p;if(!project(corner,p)){valid=false;break;}
            low.x=std::min(low.x,p.x);low.y=std::min(low.y,p.y);high.x=std::max(high.x,p.x);high.y=std::max(high.y,p.y);
        }
        if(!valid||high.x<0||high.y<0||low.x>width||low.y>height)continue;
        ImU32 color=IM_COL32(0,184,240,255);
        if(c.espVisibility)color=!n.visibilityKnown?IM_COL32(135,150,160,255):n.visible?IM_COL32(0,184,240,255):IM_COL32(245,159,68,255);
        if(c.boxes){d->AddRect({low.x-1,low.y-1},{high.x+1,high.y+1},IM_COL32(0,0,0,210));d->AddRect(low,high,color);}
        if(c.health){float f=std::clamp(n.hp/static_cast<float>(std::max(n.maxHp,1)),0.0f,1.0f);d->AddRectFilled({low.x-6,low.y-1},{low.x-2,high.y+1},IM_COL32(0,0,0,210));d->AddRectFilled({low.x-5,high.y-(high.y-low.y)*f},{low.x-3,high.y},IM_COL32(77,222,136,255));}
        auto centered=[&](const char* text,float y){ImVec2 size=ImGui::CalcTextSize(text);float x=(low.x+high.x-size.x)/2;d->AddText({x+1,y+1},IM_COL32(0,0,0,255),text);d->AddText({x,y},IM_COL32(223,237,246,255),text);};
        if(c.names)centered(n.name.c_str(),low.y-ImGui::GetFontSize()-3);
        if(c.distance){char text[64];std::snprintf(text,sizeof(text),"%.0f m | %d HP",n.meters,n.hp);centered(text,high.y+3);}
    }
}
}
