#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "SharedPair.h"

// All texture operations run on the game's existing serial native render tasks.
class PairProducer {
public:
    bool Initialize(unsigned width,unsigned height);
    void Pace();
    void SetRuntimeVariant(uint32_t value){if(header)InterlockedExchange(reinterpret_cast<volatile LONG*>(&header->reserved3),LONG(value));}
    bool Left(ID3D11Texture2D* texture);
    bool Right(ID3D11Texture2D* texture, uint64_t frame,const Transport::RenderInfo& rendered);
    bool ReadTracking(Transport::Tracking& result);
#ifdef ACUNITYVR_VISIBILITY_MASK
    bool ReadVisibility(uint32_t& generation,Transport::Visibility& result){return Transport::ReadVisibility(header,generation,result);}
#endif
    void Cancel();
    void Stop();
    bool StopRequested() const;
    uint64_t Published() const { return published; }
    uint64_t PacingWaits() const { return pacingWaits; }
    uint64_t PacingTimeouts() const { return pacingTimeouts; }
private:
    bool Prepare(ID3D11Texture2D* texture);
    HANDLE mapping{};
    HANDLE frameEvent{};
    unsigned expectedWidth{},expectedHeight{};
    uint64_t pacingWaits{},pacingTimeouts{};
    Transport::Header* header{};
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pair;
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> mutex;
    D3D11_TEXTURE2D_DESC source{};
    bool held{};
    uint64_t published{},connectedAt{};
    Transport::TrackingReader reader;
};
