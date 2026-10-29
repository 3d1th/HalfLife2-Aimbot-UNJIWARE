#include "platform.hpp"
#include <bcrypt.h>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace uw {
void* Interface(const wchar_t* module,const char* name){
    HMODULE h=GetModuleHandleW(module);if(!h)return nullptr;
    auto factory=reinterpret_cast<void*(__cdecl*)(const char*,int*)>(GetProcAddress(h,"CreateInterface"));
    return factory?factory(name,nullptr):nullptr;
}
bool Readable(const void* address,std::size_t length){
    auto p=reinterpret_cast<std::uintptr_t>(address);
    if(p<0x10000||length>0x1000000||p+length<p)return false;
    auto end=p+length;
    while(p<end){MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m)))return false;
        if(m.State!=MEM_COMMIT||(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
        auto next=reinterpret_cast<std::uintptr_t>(m.BaseAddress)+m.RegionSize;if(next<=p)return false;p=next;
    }
    return true;
}
std::wstring ModulePath(HMODULE module){
    if(!module){SetLastError(ERROR_INVALID_HANDLE);return {};}
    wchar_t path[32768]{};DWORD count=GetModuleFileNameW(module,path,32768);
    if(!count||count>=32768)return {};return {path,count};
}
std::string Utf8(const std::wstring& text){
    if(text.empty())return {};
    int count=WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    std::string result(count,'\0');WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),result.data(),count,nullptr,nullptr);return result;
}
std::string FileHash(HMODULE module,std::string* failure){
    if(failure)failure->clear();
    auto error=[&](const char* api,unsigned long code){if(failure){char text[96];std::snprintf(text,sizeof(text),"%s failed (0x%08lx)",api,code);*failure=text;}};
    auto path=ModulePath(module);if(path.empty()){error("GetModuleFileNameW",GetLastError());return {};}
    HANDLE f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE){error("CreateFileW",GetLastError());return {};}
    BCRYPT_ALG_HANDLE alg{};BCRYPT_HASH_HANDLE hash{};std::string result;
    NTSTATUS status=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0);
    if(status>=0){
        DWORD size{},received{};
        status=BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&received,0);
        if(status>=0){
            std::vector<unsigned char> object(size),buffer(65536);
            status=BCryptCreateHash(alg,&hash,object.data(),size,nullptr,0,0);
            if(status>=0){
                DWORD count{};bool ok=true;
                for(;;){if(!ReadFile(f,buffer.data(),static_cast<DWORD>(buffer.size()),&count,nullptr)){error("ReadFile",GetLastError());ok=false;break;}if(!count)break;status=BCryptHashData(hash,buffer.data(),count,0);if(status<0){error("BCryptHashData",status);ok=false;break;}}
                unsigned char digest[32]{};
                if(ok){status=BCryptFinishHash(hash,digest,sizeof(digest),0);if(status>=0){const char* hex="0123456789abcdef";for(auto c:digest){result+=hex[c>>4];result+=hex[c&15];}}else error("BCryptFinishHash",status);}
                BCryptDestroyHash(hash);
            }else error("BCryptCreateHash",status);
        }else error("BCryptGetProperty",status);
        BCryptCloseAlgorithmProvider(alg,0);
    }else error("BCryptOpenAlgorithmProvider",status);
    CloseHandle(f);return result;
}
}
