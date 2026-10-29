#include "ui.hpp"
#include <cstdio>
int main(){
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={960,640};io.DeltaTime=1.0f/60;
    io.IniFilename=nullptr;io.Fonts->AddFontDefault();
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    auto draw=[&](bool enabled,const uw::Matrix4x4* matrix){
        uw::Settings c;c.drawFov=enabled;c.silentAim=false;c.esp=false;uw::WriteSettings(c);
        uw::WriteSnapshot({});ImGui::NewFrame();uw::DrawFov(960,640,matrix);ImGui::Render();
        return ImGui::GetDrawData()->TotalVtxCount;
    };
    int on=draw(true,nullptr),off=draw(false,nullptr);
    uw::Matrix4x4 matrix{};matrix.m[0][0]=1;
    int projected=draw(true,&matrix);
    matrix.m[0][0]=NAN;int invalid=draw(true,&matrix);
    bool ok=on>0&&off==0&&projected>0&&invalid==0;
    std::printf("FOV without silent aim, ESP, snapshot or matrix: %d vertices\nDisabled FOV: %d\nProjection: %d\nInvalid projection: %d\n",on,off,projected,invalid);
    ImGui::DestroyContext();std::puts(ok?"UI PROBE PASSED":"UI PROBE FAILED");return ok?0:1;
}
