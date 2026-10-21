// Temporary first-chance fault records without debugger attachment.
// No allocation, CRT output, breakpoints or code patches in the exception handler.
#include "NativeFault48.h"
#include <Windows.h>
#include <TlHelp32.h>
#include <share.h>
#include <string>
#include <cstdint>
#include <cstring>
namespace NativeFault48 {
namespace {
constexpr unsigned Limit=8,StackSize=8192;
#pragma pack(push,1)
struct Header{char magic[8];uint32_t version,recordSize,contextSize,limit;};
struct Record{uint64_t tick;uint32_t thread,code,flags,parameters;uint64_t address,info[15];uint8_t context[sizeof(CONTEXT)];uint64_t stackBase;uint32_t stackBytes,stackError;uint8_t stack[StackSize];};
#pragma pack(pop)
static_assert(sizeof(CONTEXT)==1232 && offsetof(Record,context)==152 && offsetof(Record,stack)==1400 && sizeof(Record)==9592);
Record records[Limit]{};volatile LONG enabled{},attempts{},written{},inFlight{};PVOID handler{};HANDLE file=INVALID_HANDLE_VALUE;FILE* report{};
bool Fault(DWORD code){return code==EXCEPTION_ACCESS_VIOLATION || code==EXCEPTION_STACK_OVERFLOW || code==EXCEPTION_ILLEGAL_INSTRUCTION || code==0xc0000374 || code==0xc0000409;}
LONG CALLBACK Observe(EXCEPTION_POINTERS* exception){
 if(!exception || !exception->ExceptionRecord || !exception->ContextRecord || !Fault(exception->ExceptionRecord->ExceptionCode) || !InterlockedCompareExchange(&enabled,0,0))return EXCEPTION_CONTINUE_SEARCH;
 InterlockedIncrement(&inFlight);LONG index=InterlockedIncrement(&attempts)-1;
 if(index<static_cast<LONG>(Limit)){
  auto& record=records[index];auto& e=*exception->ExceptionRecord;auto& context=*exception->ContextRecord;
  record.tick=GetTickCount64();record.thread=GetCurrentThreadId();record.code=e.ExceptionCode;record.flags=e.ExceptionFlags;record.parameters=e.NumberParameters;record.address=reinterpret_cast<uint64_t>(e.ExceptionAddress);
  memcpy(record.info,e.ExceptionInformation,sizeof(record.info));memcpy(record.context,&context,sizeof(context));record.stackBase=context.Rsp;
  MEMORY_BASIC_INFORMATION memory{};SIZE_T toRead=0,copied{};if(VirtualQuery(reinterpret_cast<void*>(record.stackBase),&memory,sizeof(memory)) && memory.State==MEM_COMMIT){uint64_t end=reinterpret_cast<uint64_t>(memory.BaseAddress)+memory.RegionSize;if(end>record.stackBase){auto available=end-record.stackBase;toRead=available<StackSize?static_cast<SIZE_T>(available):StackSize;}}
  BOOL read=toRead && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(record.stackBase),record.stack,toRead,&copied);record.stackBytes=static_cast<uint32_t>(copied);record.stackError=read?0:GetLastError();
  DWORD bytes{};if(WriteFile(file,&record,sizeof(record),&bytes,nullptr) && bytes==sizeof(record))InterlockedIncrement(&written);
 }
 InterlockedDecrement(&inFlight);return EXCEPTION_CONTINUE_SEARCH;
}
std::string Escape(const wchar_t* text){int bytes=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);if(bytes<=0)return {};std::string utf8(static_cast<size_t>(bytes),'\0');WideCharToMultiByte(CP_UTF8,0,text,-1,utf8.data(),bytes,nullptr,nullptr);utf8.pop_back();std::string escaped;for(char c:utf8){if(c=='\\' || c=='"')escaped+='\\';escaped+=c;}return escaped;}
bool Modules(const std::filesystem::path& output){
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());if(snapshot==INVALID_HANDLE_VALUE)return false;FILE* modules=_wfsopen((output/L"native-fault48-modules.json").c_str(),L"wb",_SH_DENYNO);if(!modules){CloseHandle(snapshot);return false;}
 MODULEENTRY32W entry{};entry.dwSize=sizeof(entry);bool first=true;fprintf(modules,"[");if(Module32FirstW(snapshot,&entry))do{auto path=Escape(entry.szExePath);fprintf(modules,"%s{\"base\":\"%llx\",\"size\":%lu,\"path\":\"%s\"}",first?"":",",reinterpret_cast<uint64_t>(entry.modBaseAddr),entry.modBaseSize,path.c_str());first=false;}while(Module32NextW(snapshot,&entry));fprintf(modules,"]\n");bool ok=!first && !ferror(modules);fclose(modules);CloseHandle(snapshot);return ok;
}
}
bool Install(const std::filesystem::path& output,FILE* log){
 report=log;if(handler || file!=INVALID_HANDLE_VALUE)return false;std::error_code error;auto storage=output/(L"fault48-"+std::to_wstring(GetCurrentProcessId()));std::filesystem::create_directories(storage,error);if(error || !Modules(storage))return false;
 file=CreateFileW((storage/L"native-fault48.bin").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
 Header header{{'A','C','U','F','L','T','4','8'},1,sizeof(Record),sizeof(CONTEXT),Limit};DWORD bytes{};if(!WriteFile(file,&header,sizeof(header),&bytes,nullptr) || bytes!=sizeof(header)){CloseHandle(file);file=INVALID_HANDLE_VALUE;return false;}
 handler=AddVectoredExceptionHandler(1,Observe);if(!handler){CloseHandle(file);file=INVALID_HANDLE_VALUE;return false;}InterlockedExchange(&enabled,1);
 fprintf(report,"Temporary native fault48 recorder installed: first-chance fault registers/8 KiB stack only, max8, no debugger attachment/code patches; always continues native exception handling. Path=%ls\n",storage.c_str());fflush(report);return true;
}
void Stop(){
 InterlockedExchange(&enabled,0);bool removed=!handler || RemoveVectoredExceptionHandler(handler)!=0;if(removed)handler=nullptr;
 if(report) {fprintf(report,"Native fault48 recorder retired handlerRemoved=%d attempts=%ld written=%ld inFlight=%ld; fault buffers/file retained until process exit.\n",removed,attempts,written,inFlight);fflush(report);}
 // Windows reclaims the file on process exit; retaining it avoids racing a handler already dispatched.
}
}
