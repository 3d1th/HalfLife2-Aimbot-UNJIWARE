#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <cstddef>
#include <string>

namespace uw {
void* Interface(const wchar_t* module,const char* name);
bool Readable(const void* address,std::size_t length);
std::wstring ModulePath(HMODULE module);
std::string Utf8(const std::wstring& text);
std::string FileHash(HMODULE module,std::string* failure=nullptr);
}
