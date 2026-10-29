#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include <string>
#include <vector>

namespace {
std::uintptr_t RemoteModule(DWORD pid,const wchar_t* name){
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    if(snapshot==INVALID_HANDLE_VALUE)return 0;
    MODULEENTRY32W entry{sizeof(entry)};std::uintptr_t result=0;
    if(Module32FirstW(snapshot,&entry))do{if(_wcsicmp(entry.szModule,name)==0){result=reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);break;}}while(Module32NextW(snapshot,&entry));
    CloseHandle(snapshot);return result;
}
int Fail(const char* message){std::fprintf(stderr,"%s (Win32 error %lu)\n",message,GetLastError());return 1;}
}
int main(int argc,char** argv){
    const bool status=argc>1&&std::string(argv[1])=="--status";
    std::vector<DWORD> processes;
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return Fail("Process enumeration failed");
    PROCESSENTRY32W entry{sizeof(entry)};
    if(Process32FirstW(snapshot,&entry))do{if(_wcsicmp(entry.szExeFile,L"hl2.exe")==0)processes.push_back(entry.th32ProcessID);}while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);
    if(processes.empty()){std::puts("Half-Life 2 is not running. Start HL2, then run this loader.");return 2;}
    if(processes.size()!=1){std::puts("Multiple hl2.exe processes found; close extra instances before injecting.");return 2;}
    const DWORD pid=processes.front();std::printf("hl2.exe PID: %lu\n",pid);
    if(status)return 0;
    if(RemoteModule(pid,L"UNJIWARE.dll")){std::puts("UNJIWARE is already loaded. Press INSERT in the game.");return 0;}
    wchar_t filename[32768]{};GetModuleFileNameW(nullptr,filename,32768);std::wstring dll(filename);dll.resize(dll.find_last_of(L"\\/")+1);dll+=L"UNJIWARE.dll";
    if(GetFileAttributesW(dll.c_str())==INVALID_FILE_ATTRIBUTES)return Fail("Place UNJIWARE.dll beside the loader");
    HANDLE process=OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid);
    if(!process)return Fail("Could not open hl2.exe");
    const SIZE_T bytes=(dll.size()+1)*sizeof(wchar_t);void* remote=VirtualAllocEx(process,nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!remote){CloseHandle(process);return Fail("Remote allocation failed");}
    auto local=reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW"));
    HMODULE owner=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(local),&owner);
    wchar_t ownerPath[MAX_PATH]{};GetModuleFileNameW(owner,ownerPath,MAX_PATH);
    const wchar_t* name=wcsrchr(ownerPath,L'\\');name=name?name+1:ownerPath;
    auto remoteBase=RemoteModule(pid,name);
    auto load=reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteBase+(reinterpret_cast<std::uintptr_t>(local)-reinterpret_cast<std::uintptr_t>(owner)));
    SIZE_T written{};
    if(!local||!owner||!remoteBase||!WriteProcessMemory(process,remote,dll.c_str(),bytes,&written)||written!=bytes){VirtualFreeEx(process,remote,0,MEM_RELEASE);CloseHandle(process);return Fail("Could not prepare DLL load");}
    HANDLE thread=CreateRemoteThread(process,nullptr,0,load,remote,0,nullptr);
    if(!thread){VirtualFreeEx(process,remote,0,MEM_RELEASE);CloseHandle(process);return Fail("LoadLibrary thread creation failed");}
    DWORD wait=WaitForSingleObject(thread,15000),result{};
    if(wait!=WAIT_OBJECT_0){CloseHandle(thread);CloseHandle(process);std::puts("DLL load is still pending. Check UNJIWARE.log before retrying.");return 3;}
    GetExitCodeThread(thread,&result);CloseHandle(thread);VirtualFreeEx(process,remote,0,MEM_RELEASE);CloseHandle(process);
    if(!result)return Fail("LoadLibraryW failed in hl2.exe");
    std::puts("UNJIWARE loaded. Press INSERT to toggle the menu. See UNJIWARE.log for feature readiness.");return 0;
}
