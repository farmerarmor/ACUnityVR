#include "BuildGuard.h"
#include "ShaderObserve75.h"
#include "ShaderRegistry92.h"
#include "MaterialProgramSpecs75.h"
#include <map>
#include <string>
#include <cstdio>
#include <share.h>
extern "C" uint32_t WINAPI ACUnityShaderRegistrySnapshot(ShaderRegistry92::Entry*,uint32_t,ShaderRegistry92::State*);
namespace {
HMODULE self{};FILE* log{};uint8_t* image{};
bool Read(void* out,uint64_t p,size_t n)noexcept{if(!out||p<0x10000||p>0x00007ffffffeffffULL||!n||n>0x10000)return false;__try{memcpy(out,reinterpret_cast<void*>(p),n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Q(uint64_t p,size_t off,uint64_t& v){return Read(&v,p+off,8);}
bool Same(uint64_t p,size_t off,uint64_t v){uint64_t got{};return Q(p,off,got)&&got==v;}
bool Gameplay(){uint64_t surface{},list{},view{},graph{},renderer{},world{},scene{},collection{},component{},player{},director{},bulk{},selector{};uint16_t count{};uint32_t mode{};uint8_t blend{};auto base=uint64_t(image);
 return Q(base,0x523c6d8,surface)&&Read(&count,surface+0x32,2)&&count==1&&Q(surface,0x28,list)&&Q(list,0,view)&&Same(view,0x420,surface)&&Same(view,0x248,0)&&Q(view,0x440,graph)&&Same(graph,0,view)&&Q(graph,0x10,renderer)&&Same(renderer,0,base+0x3094180)&&Q(renderer,0x28,world)&&Same(world,0,base+0x30225e0)&&Q(renderer,0x1e8,scene)&&Same(scene,8,view)&&
 Q(base,0x521aad0,collection)&&Same(collection,0,base+0x304d800)&&Read(&count,collection+0x4a,2)&&count==1&&Q(collection,0x40,list)&&Q(list,0,component)&&Same(component,0,base+0x3050b00)&&Same(component,0x120,base+0x4eec760)&&Q(component,0x10,player)&&Same(player,0,base+0x3017d60)&&Same(player,0x60,world)&&Same(world,0xd8,player)&&
 Q(base,0x521a768,director)&&Same(director,0,base+0x3040d10)&&Read(&mode,director+0x64,4)&&mode==1&&Read(&blend,director+0x58,1)&&!blend&&Q(world,0x998,bulk)&&Same(bulk,0,base+0x304ab90)&&Q(bulk,0x4f8,selector)&&Same(selector,0,base+0x2f6b980)&&Same(selector,0x80,bulk);
}
bool MaterialsReady(){
 std::vector<ShaderRegistry92::Entry> entries(ShaderRegistry92::Limit);ShaderRegistry92::State state{};auto n=ACUnityShaderRegistrySnapshot(entries.data(),unsigned(entries.size()),&state);
 if(!n||n>entries.size()||state.failures)return false;std::map<uint64_t,unsigned> devices;
 for(unsigned i=0;i<n;++i){auto& e=entries[i];unsigned bit{};
  for(auto group:{1u,5u,6u})for(unsigned stage=0;stage<2;++stage,++bit){auto expected=stage?MaterialProgramSpecs75::specs[group].ps:MaterialProgramSpecs75::specs[group].vs;if(e.stage==stage&&!strcmp(e.hash,expected))devices[e.device]|=1u<<bit;}
 }
 for(auto& pair:devices)if(pair.second==63)return true;return false;
}
bool BeginHost(const std::filesystem::path& folder){auto exe=folder/L"ACUnityVRHost.exe";auto command=L"\""+exe.wstring()+L"\" "+std::to_wstring(GetCurrentProcessId());STARTUPINFOW start{sizeof(start)};PROCESS_INFORMATION process{};
 if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,folder.c_str(),&start,&process)){fprintf(log,"ERROR could not start VR host, Win32=%lu\n",GetLastError());fflush(log);return false;}
 if(WaitForSingleObject(process.hProcess,1000)==WAIT_OBJECT_0){DWORD result{};GetExitCodeProcess(process.hProcess,&result);fprintf(log,"ERROR VR host exited during startup, code=%lu; inspect OpenXR log\n",result);fflush(log);CloseHandle(process.hThread);CloseHandle(process.hProcess);return false;}
 fprintf(log,"VR host started pid=%lu; no stop hotkey, close Unity normally to end the session\n",process.dwProcessId);fflush(log);CloseHandle(process.hThread);CloseHandle(process.hProcess);return true;
}
DWORD WINAPI Run(void*){
 wchar_t filename[32768]{};if(!GetModuleFileNameW(self,filename,_countof(filename)))return 1;auto folder=std::filesystem::path(filename).parent_path();log=_wfsopen((folder/L"ACUnityVR-startup.log").c_str(),L"w",_SH_DENYNO);if(!log)return 2;
 try{
  fprintf(log,"Automatic startup pid=%lu; in-memory shader identities; no manual collector or shader dumps\n",GetCurrentProcessId());fflush(log);
  if(!acu::VerifyImage()){fprintf(log,"ERROR unsupported executable\n");fflush(log);return 3;}image=reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
  while(!GetModuleHandleW(L"d3d11.dll"))Sleep(10);
  if(!ShaderObserve75::Start(folder)){fprintf(log,"ERROR shader registry could not start\n");fflush(log);return 4;}
  fprintf(log,"Shader registry active. Waiting for loaded gameplay automatically.\n");fflush(log);
  unsigned stable{};uint64_t materialWait{};
  // Loading a save is user-controlled and has no timeout.
  while(stable<3){auto stats=ShaderObserve75::Stats();if(stats.failures||stats.budgetRefused){ShaderObserve75::Stop();fprintf(log,"ERROR shader identification failed or exceeded its bounded registry\n");fflush(log);return 5;}
   bool gameplay=Gameplay(),materials=gameplay&&MaterialsReady();auto now=GetTickCount64();
   if(gameplay&&!materials){if(!materialWait)materialWait=now;if(now-materialWait>30000){ShaderObserve75::Stop();fprintf(log,"ERROR required shader identities were missed; close Unity and restart through the VR launcher\n");fflush(log);return 12;}}else materialWait=0;
   stable=gameplay&&materials?stable+1:0;Sleep(200);
  }
  if(!ShaderObserve75::Stop()){fprintf(log,"ERROR shader hooks could not stop\n");fflush(log);return 6;}
  std::vector<ShaderRegistry92::Entry> entries;if(!ShaderRegistry92::Read(entries)){fprintf(log,"ERROR shader registry snapshot invalid\n");fflush(log);return 7;}
  fprintf(log,"Gameplay ready; registry stopped automatically, pinned identities=%zu. Starting native VR.\n",entries.size());fflush(log);
  auto readyName=L"Local\\ACUnityVRReady-"+std::to_wstring(GetCurrentProcessId()),failedName=L"Local\\ACUnityVRFailed-"+std::to_wstring(GetCurrentProcessId());
  HANDLE events[]={CreateEventW(nullptr,TRUE,FALSE,readyName.c_str()),CreateEventW(nullptr,TRUE,FALSE,failedName.c_str())};
  if(!events[0]||!events[1]){fprintf(log,"ERROR startup events\n");fflush(log);return 8;}
  auto module=LoadLibraryExW((folder/L"ACUnityVRNative.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  auto status=module?WaitForMultipleObjects(2,events,FALSE,30000):WAIT_FAILED;CloseHandle(events[0]);CloseHandle(events[1]);
  if(status!=WAIT_OBJECT_0){fprintf(log,"ERROR native VR initialization failed or timed out; inspect ACUnityVR-native.log; restart the game before retrying\n");fflush(log);return 9;}
  return BeginHost(folder)?0:10;
 }catch(...){fprintf(log,"ERROR automatic startup exception\n");fflush(log);return 11;}
}
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){self=module;DisableThreadLibraryCalls(module);auto thread=CreateThread(nullptr,0,Run,nullptr,0,nullptr);if(!thread)return FALSE;CloseHandle(thread);}return TRUE;}
