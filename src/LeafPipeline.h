#pragma once
#include <cstdint>
namespace LeafPipeline {
constexpr unsigned None=UINT32_MAX;
struct State {
 unsigned open=None,begun{},overlaps{},forced{};
 bool Begin(unsigned token,unsigned kind,bool capture){
  if(!capture || token==None || (kind!=1 && kind!=3 && kind!=4))return false;
  if(open!=None){++overlaps;return false;}
  open=token;++begun;return true;
 }
 bool End(unsigned token){if(token!=open || open==None)return false;open=None;return true;}
 unsigned ForceClose(){auto result=open;if(open!=None){open=None;++forced;}return result;}
};
}
