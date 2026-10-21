#include "NativeBridge.h"
#include "HostCopyTiming.h"
#include <wrl/client.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <share.h>

using Microsoft::WRL::ComPtr;
struct Connection {
    HANDLE game{},mapping{};
    Transport::Header* header{};
    ~Connection() {
        if(header){InterlockedExchange(reinterpret_cast<volatile LONG*>(&header->reserved2),1);UnmapViewOfFile(header);}
        if(mapping)CloseHandle(mapping);
        if(game)CloseHandle(game);
    }
};
int wmain(int argc,wchar_t** argv) {
    if(argc==2 && !wcscmp(argv[1],L"--display-query")) {
        HeadsetDisplay::Settings s{};
        if(!NativeBridge::QueryDisplaySettings(s))return 2;
        printf("OpenXR eye=%ux%u refresh=%.3f Hz\n",s.width,s.height,s.refreshHz);return 0;
    }
    if(argc!=2){fwprintf(stderr,L"Usage: ACUnityVRHost.exe <ACU PID>\n");return 1;}
    DWORD pid=wcstoul(argv[1],nullptr,10);
    wchar_t name[96];Transport::Name(name,pid);
    Connection connection;
    HANDLE game=connection.game=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    HANDLE mapping{};
    auto deadline=GetTickCount64()+15000;
    while(game && !mapping && GetTickCount64()<deadline && WaitForSingleObject(game,50)==WAIT_TIMEOUT)
        mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name);
    connection.mapping=mapping;
    if(!game || !mapping){printf("Game or native pair mapping unavailable: %lu\n",GetLastError());return 3;}
    auto* h=connection.header=static_cast<Transport::Header*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Transport::Header)));
    if(!h || h->magic!=Transport::Magic || h->version!=Transport::Version || h->pid!=pid)return 4;
    auto stopFlag=reinterpret_cast<volatile LONG*>(&h->reserved2);
    while(!h->generation && !InterlockedCompareExchange(stopFlag,0,0) && WaitForSingleObject(game,10)==WAIT_TIMEOUT){}
    if(!h->generation || !h->width || h->width>16384 || !h->height || h->height>8192)return 5;
    ComPtr<IDXGIFactory1> factory;ComPtr<IDXGIAdapter1> selected;
    if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return 6;
    for(UINT i=0;;++i) {
        ComPtr<IDXGIAdapter1> a;if(factory->EnumAdapters1(i,&a)==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 d{};
        if(a && SUCCEEDED(a->GetDesc1(&d)) && !memcmp(&d.AdapterLuid,&h->adapter,sizeof(LUID))){selected=a;break;}
    }
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    if(!selected || FAILED(D3D11CreateDevice(selected.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)))return 7;
    ComPtr<ID3D11Texture2D> shared,local;ComPtr<IDXGIKeyedMutex> mutex;
    if(FAILED(device->OpenSharedResource(reinterpret_cast<HANDLE>(h->sharedHandle),IID_PPV_ARGS(&shared))) || FAILED(shared.As(&mutex)))return 8;
    D3D11_TEXTURE2D_DESC d{};shared->GetDesc(&d);
    if(!NativeBridge::ValidatePair(d,h->height) || d.Width!=h->width || uint32_t(d.Format)!=h->format)return 9;
    d.MiscFlags=0;
    if(FAILED(device->CreateTexture2D(&d,nullptr,&local)))return 10;
    if(!HostCopyTiming::Initialize(device.Get(),context.Get())){printf("Host timing unavailable; rendering continues.\n");}
    NativeBridge::SetTrackingChannel(h);
    uint64_t frame{},fresh{},repeats{},lastTick=GetTickCount64();Transport::RenderInfo info{};
    uint64_t previousFresh{},previousSubmitted{};DWORD foregroundPID{};GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPID);bool previousForeground=foregroundPID==pid;unsigned focusChanges{},modeChanges{},previousMode{};bool captureObserved{};
    FILE* rates=_fsopen("native-pair-rate.jsonl","w",_SH_DENYNO);
    FILE* log=_fsopen("ACUnityVR-host.log","w",_SH_DENYNO);
    if(log){fprintf(log,"Native pair host PID=%lu source=%ux%u format=%u ABI=%u\n",pid,h->width,h->height,h->format,Transport::Version);fflush(log);}
    while(WaitForSingleObject(game,0)==WAIT_TIMEOUT && !InterlockedCompareExchange(stopFlag,0,0)) {
#ifndef ACUNITYVR_PORTABLE92
        if(GetAsyncKeyState(VK_F10)&0x8000){InterlockedExchange(stopFlag,1);break;}
#endif
        foregroundPID=0;GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPID);bool gameForeground=foregroundPID==pid;if(gameForeground!=previousForeground){++focusChanges;previousForeground=gameForeground;}
        if(GetAsyncKeyState(VK_F8)&0x8000)captureObserved=true;
        HostCopyTiming::BeginIteration(frame,info.mode,h->reserved3,gameForeground);
        HRESULT acquired{};
        {HostCopyTiming::CpuScope timing(HostCopyTiming::Phase::AcquirePair);acquired=mutex->AcquireSync(1,2);}
        if(acquired==S_OK) {
            HostCopyTiming::FreshPair(h->frameId,h->rendered.mode);
            HostCopyTiming::BeginShared();context->CopyResource(local.Get(),shared.Get());HostCopyTiming::EndShared();
            {HostCopyTiming::CpuScope timing(HostCopyTiming::Phase::SharedFlush);context->Flush();}
            frame=h->frameId;info=h->rendered;
            mutex->ReleaseSync(0);++fresh;
        } else if(acquired!=WAIT_TIMEOUT){if(log)fprintf(log,"AcquireSync failed: %08lx\n",acquired);break;}
        else ++repeats;
        if(frame){NativeBridge::SetSourceFrame(frame);NativeBridge::SetRenderInfo(info);NativeBridge::Present(nullptr,local.Get(),h->height,h->swapEyes!=0);}
        else Sleep(5);
        HostCopyTiming::EndIteration();
        if(info.mode!=previousMode){++modeChanges;previousMode=info.mode;}
        auto now=GetTickCount64();if(now-lastTick>=5000) {
            auto submitted=NativeBridge::SubmittedPairs();const double seconds=double(now-lastTick)/1000.0,freshHz=double(fresh-previousFresh)/seconds,submissionHz=double(submitted-previousSubmitted)/seconds;
            if(log){fprintf(log,"freshPairs=%llu repeatedPolls=%llu submitted=%llu sourceFrame=%llu freshHz=%.3f submissionHz=%.3f gameForeground=%d focusChanges=%u elapsedMs=%llu\n",fresh,repeats,submitted,frame,freshHz,submissionHz,gameForeground,focusChanges,now-lastTick);fflush(log);}
            if(rates){fprintf(rates,"{\"wallTickMs\":%llu,\"elapsedMs\":%llu,\"sourceFrame\":%llu,\"mode\":%u,\"freshPairs\":%llu,\"freshPairsDelta\":%llu,\"freshHz\":%.6f,\"submitted\":%llu,\"submissionHz\":%.6f,\"variant\":%u,\"gameForeground\":%s,\"focusChanges\":%u,\"modeChanges\":%u,\"captureObserved\":%s}\n",now,now-lastTick,frame,info.mode,fresh,fresh-previousFresh,freshHz,submitted,submissionHz,h->reserved3,gameForeground?"true":"false",focusChanges,modeChanges,captureObserved?"true":"false");fflush(rates);}
            previousFresh=fresh;previousSubmitted=submitted;focusChanges=modeChanges=0;captureObserved=false;lastTick=now;
        }
    }
    InterlockedExchange(stopFlag,1);
    HostCopyTiming::Stop();NativeBridge::SetTrackingChannel(nullptr);NativeBridge::Shutdown();if(rates)fclose(rates);
    if(log){DWORD code{};BOOL queried=GetExitCodeProcess(game,&code);fprintf(log,"Stopped. freshPairs=%llu repeatedPolls=%llu gameExitCodeAvailable=%d gameExitCode=%08lx (00000103=still running)\n",fresh,repeats,queried,code);fclose(log);}
    return 0;
}
