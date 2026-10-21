// Nonblocking sparse timing on the companion's existing D3D11 context only.
#include "HostCopyTiming.h"
#include <windows.h>
#include <wrl/client.h>
#include <array>
#include <cstdio>
#include <share.h>
using Microsoft::WRL::ComPtr;
namespace HostCopyTiming {
namespace {
struct Sample {
 ComPtr<ID3D11Query> frequency;std::array<ComPtr<ID3D11Query>,4> marks;
 std::array<double,unsigned(Phase::Count)> cpu{};
 uint64_t tick{},frame{};uint32_t mode{},variant{},mask{},beginMask{};
 bool pending{},foreground{},fresh{},capture{};
};
std::array<Sample,8> ring;Sample* active{};ComPtr<ID3D11DeviceContext> context;
FILE* file{};uint64_t last{},completed{},drops{},errors{};LONGLONG cpuFrequency{};
void Poll(){for(auto& s:ring)if(s.pending){
 D3D11_QUERY_DATA_TIMESTAMP_DISJOINT frequency{};auto status=context->GetData(s.frequency.Get(),&frequency,sizeof(frequency),D3D11_ASYNC_GETDATA_DONOTFLUSH);
 if(status==S_FALSE)continue;if(FAILED(status) || frequency.Disjoint || !frequency.Frequency){++errors;s.pending=false;continue;}
 UINT64 marks[4]{};bool wait=false,failed=false;
 for(unsigned i=0;i<4;++i)if(s.mask&(1u<<(i/2))){auto result=context->GetData(s.marks[i].Get(),&marks[i],sizeof(marks[i]),D3D11_ASYNC_GETDATA_DONOTFLUSH);if(result==S_FALSE){wait=true;break;}if(FAILED(result)){failed=true;break;}}
 if(wait)continue;if(failed){++errors;s.pending=false;continue;}
 fprintf(file,"{\"event\":\"copySample\",\"wallTickMs\":%llu,\"sourceFrame\":%llu,\"mode\":%u,\"variant\":%u,\"gameForeground\":%s,\"freshPair\":%s,\"capture\":%s,\"mask\":%u,\"gpuCopies\":[",s.tick,s.frame,s.mode,s.variant,s.foreground?"true":"false",s.fresh?"true":"false",s.capture?"true":"false",s.mask);
 bool comma=false;for(unsigned i=0;i<2;++i)if(s.mask&(1u<<i)){if(marks[i*2+1]<marks[i*2]){++errors;continue;}double ms=double(marks[i*2+1]-marks[i*2])*1000./double(frequency.Frequency);fprintf(file,"%s{\"region\":\"%s\",\"gpuMs\":%.9g}",comma?",":"",i?"privatePairToXrSlices":"sharedPairToPrivate",ms);comma=true;}
 fprintf(file,"],\"cpuMs\":{");const char* names[]{"acquirePair","sharedFlush","waitFrame","waitSwapchain","swapchainFlush","endFrame"};
 for(unsigned i=0;i<unsigned(Phase::Count);++i)fprintf(file,"%s\"%s\":%.9g",i?",":"",names[i],s.cpu[i]);fprintf(file,"}}\n");fflush(file);++completed;s.pending=false;
}}
void BeginMark(unsigned bit,unsigned index){if(active && !(active->beginMask&bit)){context->End(active->marks[index].Get());active->beginMask|=bit;}}
void EndMark(unsigned bit,unsigned index){if(active && (active->beginMask&bit) && !(active->mask&bit)){context->End(active->marks[index].Get());active->mask|=bit;}}
}
bool Initialize(ID3D11Device* device,ID3D11DeviceContext* c){
 if(!device || !c || c->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;context=c;LARGE_INTEGER f{};QueryPerformanceFrequency(&f);cpuFrequency=f.QuadPart;
 for(auto& s:ring){D3D11_QUERY_DESC d{D3D11_QUERY_TIMESTAMP_DISJOINT,0};if(FAILED(device->CreateQuery(&d,&s.frequency)))return false;d.Query=D3D11_QUERY_TIMESTAMP;for(auto& q:s.marks)if(FAILED(device->CreateQuery(&d,&q)))return false;}
 file=_fsopen("host-copy-timing.jsonl","w",_SH_DENYNO);if(!file)return false;
 fprintf(file,"{\"event\":\"schema\",\"samplingMs\":1000,\"ring\":8,\"readMode\":\"DONOTFLUSH\",\"variants\":{\"1\":\"compiledClean\",\"2\":\"profile\"},\"note\":\"GPU copies timed separately from CPU waits; existing host context only; no extra Flush or wait\"}\n");fflush(file);return true;
}
void BeginIteration(uint64_t frame,uint32_t mode,uint32_t variant,bool foreground){
 if(!file || active)return;Poll();auto now=GetTickCount64();if(now-last<1000)return;last=now;
 for(auto& s:ring)if(!s.pending){s.tick=now;s.frame=frame;s.mode=mode;s.variant=variant;s.foreground=foreground;s.fresh=s.capture=false;s.mask=s.beginMask=0;s.cpu={};active=&s;context->Begin(s.frequency.Get());return;}++drops;
}
void FreshPair(uint64_t frame,uint32_t mode){if(active){active->fresh=true;active->frame=frame;active->mode=mode;}}
void BeginShared(){BeginMark(1,0);}void EndShared(){EndMark(1,1);}
void BeginSwapchain(){BeginMark(2,2);}void EndSwapchain(){EndMark(2,3);}
void Capture(){if(active)active->capture=true;}
void EndIteration(){if(active){context->End(active->frequency.Get());active->pending=true;active=nullptr;}}
CpuScope::CpuScope(Phase p):phase(p){if(active){LARGE_INTEGER t{};QueryPerformanceCounter(&t);begin=t.QuadPart;}}
CpuScope::~CpuScope(){if(begin && active){LARGE_INTEGER t{};QueryPerformanceCounter(&t);active->cpu[unsigned(phase)]+=double(t.QuadPart-begin)*1000./double(cpuFrequency);}}
void Stop(){EndIteration();if(file){Poll();unsigned pending{};for(auto& s:ring)pending+=s.pending?1:0;fprintf(file,"{\"event\":\"stop\",\"completed\":%llu,\"ringDrops\":%llu,\"errors\":%llu,\"pending\":%u}\n",completed,drops,errors,pending);fclose(file);file=nullptr;}}
}
