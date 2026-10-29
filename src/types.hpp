#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace uw {
struct Vec3 {
    float x{}, y{}, z{};
    Vec3 operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec3 operator*(float k) const { return {x*k,y*k,z*k}; }
    float Dot(Vec3 b) const { return x*b.x+y*b.y+z*b.z; }
    float Length() const { return std::sqrt(Dot(*this)); }
    Vec3 Normalized() const { float n=Length(); return n>0.00001f ? *this*(1.0f/n) : Vec3{}; }
    bool Finite() const { return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z); }
};
struct Matrix3x4 { float m[3][4]; Vec3 Transform(Vec3 v) const {
    return {v.x*m[0][0]+v.y*m[0][1]+v.z*m[0][2]+m[0][3],
            v.x*m[1][0]+v.y*m[1][1]+v.z*m[1][2]+m[1][3],
            v.x*m[2][0]+v.y*m[2][1]+v.z*m[2][2]+m[2][3]};
}};
struct Matrix4x4 { float m[4][4]; };
inline float AngleBetween(Vec3 a,Vec3 b) {
    return std::acos(std::clamp(a.Normalized().Dot(b.Normalized()),-1.0f,1.0f))*57.2957795f;
}
inline Vec3 Forward(Vec3 angles) {
    float p=angles.x*0.01745329252f,y=angles.y*0.01745329252f;
    return {std::cos(p)*std::cos(y),std::cos(p)*std::sin(y),-std::sin(p)};
}
template<class T> T& Field(void* object,std::size_t offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(object)+offset);
}
template<class R,class... Args> R VCall(void* object,std::size_t index,Args... args) {
    using Fn=R(__thiscall*)(void*,Args...);
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[index])(object,args...);
}
}
