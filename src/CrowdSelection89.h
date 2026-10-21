#pragma once
// F8-only CPU census around the existing owned BulkWorld render callback.
// No hooks, GPU commands, native calls or game writes are introduced here.
#include "EffectInputs72.h"
#include <windows.h>
#include <array>
#include <atomic>
#include <filesystem>
#include <cstdio>
#include <share.h>
namespace CrowdSelection89 {
constexpr unsigned Capacity=8192,Stride=0x180,Views=3;
struct Record {
 uint64_t buffer{},view{},logicalView{},input{},selection{};
 unsigned which{},inputCount{},selectedCount{},errors{},complete{};
 uint8_t before[0xc8]{},after[0xc8]{},camera[0x440]{},planes[0xc0]{};
 std::array<uint8_t,Capacity*Stride> inputBefore{},inputAfter{};
 std::array<uint64_t,Capacity> selected{};
};
struct Token {uint64_t epoch{};unsigned slot{};explicit operator bool()const{return epoch!=0;}};
class Capture {
 SRWLOCK mutex=SRWLOCK_INIT;
 struct Lock {SRWLOCK* p;Lock(SRWLOCK& m):p(&m){AcquireSRWLockExclusive(p);}~Lock(){ReleaseSRWLockExclusive(p);}};
 std::array<Record,Views> records{};uint64_t epoch{},sequence{},tracking{};unsigned frame{},claimed{},ignored{};std::atomic<bool> active{};
 std::filesystem::path path;
 template<class T>static T Get(const uint8_t* p,size_t at){return EffectInputs72::Get<T>(p,at);}
 static bool Copy(EffectInputs72::Reader read,void* out,uint64_t address,size_t bytes){
  if(!bytes)return true;if(!address || address+bytes<address)return false;
  auto p=static_cast<uint8_t*>(out);while(bytes){size_t n=bytes<0xf000?bytes:0xf000;if(!read(p,address,n))return false;p+=n;address+=n;bytes-=n;}return true;
 }
 static bool Write(const std::filesystem::path& p,const void* data,size_t bytes){FILE* f=_wfsopen(p.c_str(),L"wb",_SH_DENYNO);if(!f)return false;bool ok=!bytes || fwrite(data,bytes,1,f)==1;return fclose(f)==0&&ok;}
 static void Hex(FILE* f,const uint8_t* p,size_t n){for(size_t i=0;i<n;++i)fprintf(f,"%02x",p[i]);}
public:
 void Arm(uint64_t id,uint64_t pose,unsigned sourceFrame,const std::filesystem::path& output){Lock guard(mutex);active=false;if(!++epoch)++epoch;sequence=id;tracking=pose;frame=sourceFrame;path=output;claimed=ignored=0;for(auto& r:records){r.which=r.complete=r.errors=0;}active=true;}
 Token Begin(EffectInputs72::Reader read,uint64_t buffer,uint64_t view,uint64_t logicalView,unsigned which){
  if(!active.load(std::memory_order_relaxed))return {};Lock guard(mutex);if(!active || !read || which<1 || which>Views)return {};auto& r=records[which-1];if(r.which){++ignored;return {};}r.which=which;++claimed;r.complete=r.errors=r.inputCount=r.selectedCount=0;r.buffer=buffer;r.view=view;r.logicalView=logicalView;r.input=r.selection=0;
  memset(r.before,0,sizeof(r.before));memset(r.after,0,sizeof(r.after));memset(r.camera,0,sizeof(r.camera));memset(r.planes,0,sizeof(r.planes));
  Token token{epoch,which-1};if(!read(r.before,buffer,sizeof(r.before))){r.errors|=1;return token;}
  r.input=Get<uint64_t>(r.before,0);r.inputCount=Get<unsigned>(r.before,0x40);
  if(r.inputCount>Capacity || (r.inputCount && !r.input)){r.errors|=2;return token;}
  if(!read(r.camera,view,sizeof(r.camera)) || !read(r.planes,Get<uint64_t>(r.camera,0x428),sizeof(r.planes))){r.errors|=4;return token;}
  if(!Copy(read,r.inputBefore.data(),r.input,size_t(r.inputCount)*Stride)){r.errors|=8;return token;}
  uint8_t check[0xc8]{};if(!read(check,buffer,sizeof(check)) || Get<uint64_t>(check,0)!=r.input || Get<unsigned>(check,0x40)!=r.inputCount)r.errors|=16;
  return token;
 }
 void End(EffectInputs72::Reader read,Token token,bool ownerValid){
  if(!token)return;Lock guard(mutex);if(!active || token.epoch!=epoch || token.slot>=Views)return;auto& r=records[token.slot];r.complete=1;
  if(!ownerValid){r.errors|=32;return;}if(r.errors)return;
  if(!read(r.after,r.buffer,sizeof(r.after)) || Get<uint64_t>(r.after,0)!=r.input || Get<unsigned>(r.after,0x40)!=r.inputCount){r.errors|=64;return;}
  r.selection=Get<uint64_t>(r.after,0x90);r.selectedCount=Get<unsigned>(r.after,0x98);
  if(r.selectedCount>Capacity || (r.selectedCount && !r.selection)){r.errors|=128;return;}
  if(!Copy(read,r.inputAfter.data(),r.input,size_t(r.inputCount)*Stride) || !Copy(read,r.selected.data(),r.selection,size_t(r.selectedCount)*8)){r.errors|=256;return;}
  uint8_t check[0xc8]{};if(!read(check,r.buffer,sizeof(check)) || Get<uint64_t>(check,0)!=r.input || Get<unsigned>(check,0x40)!=r.inputCount || Get<uint64_t>(check,0x90)!=r.selection || Get<unsigned>(check,0x98)!=r.selectedCount){r.errors|=512;return;}
  for(unsigned i=0;i<r.selectedCount;++i)if((r.selected[i]&0x7fff)>=r.inputCount){r.errors|=1024;return;}
 }
 void Finish(){Lock guard(mutex);if(!active)return;active=false;auto f=_wfsopen((path/L"crowd-selection89.json").c_str(),L"w",_SH_DENYNO);if(!f)return;
  fprintf(f,"{\"sequence\":%llu,\"tracking\":%llu,\"sourceFrame\":%u,\"capacity\":%u,\"stride\":%u,\"claimed\":%u,\"duplicateCallbacksIgnored\":%u,\"note\":\"One owned source/left/right callback per F8 epoch. Inputs copied before native selection/building and after the original callback once; selected encoded indices copied after. CPU memory only. Missing/incomplete/overflow snapshots are explicitly invalid. Tail/animation matrices must be interpreted with native member counts.\",\"views\":[",sequence,tracking,frame,Capacity,Stride,claimed,ignored);bool comma=false;
  for(auto& r:records){if(!r.which)continue;auto prefix=path/(L"crowd-selection89-view-"+std::to_wstring(r.which));bool valid=r.complete && !r.errors;bool ok=false;
   if(valid)ok=Write(prefix.wstring()+L"-before.bin",r.inputBefore.data(),size_t(r.inputCount)*Stride) && Write(prefix.wstring()+L"-after.bin",r.inputAfter.data(),size_t(r.inputCount)*Stride) && Write(prefix.wstring()+L"-selected.bin",r.selected.data(),size_t(r.selectedCount)*8) && Write(prefix.wstring()+L"-camera.bin",r.camera,sizeof(r.camera)) && Write(prefix.wstring()+L"-planes.bin",r.planes,sizeof(r.planes));
   fprintf(f,"%s{\"whichView\":%u,\"view\":\"%llx\",\"logicalView\":\"%llx\",\"buffer\":\"%llx\",\"input\":\"%llx\",\"inputCount\":%u,\"selected\":\"%llx\",\"selectedCount\":%u,\"complete\":%s,\"errors\":%u,\"filesOK\":%s,\"headerBefore\":\"",comma?",":"",r.which,r.view,r.logicalView,r.buffer,r.input,r.inputCount,r.selection,r.selectedCount,r.complete?"true":"false",r.errors,ok?"true":"false");Hex(f,r.before,sizeof(r.before));fprintf(f,"\",\"headerAfter\":\"");Hex(f,r.after,sizeof(r.after));fprintf(f,"\"}");comma=true;
  }fprintf(f,"]}\n");fclose(f);
 }
 void Stop(){Lock guard(mutex);active=false;if(!++epoch)++epoch;}
};
}
