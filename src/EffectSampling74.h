#pragma once
// F8 samples representative shader/pass families throughout both eyes.
#include <array>
#include <cstdint>
#include <vector>
#include <cstring>
namespace EffectSampling74 {
constexpr unsigned Eyes=2,FamiliesPerEye=256,WorkerFamilies=192,DirectFamilies=64,SamplesPerFamily=4,RecordLimit=Eyes*FamiliesPerEye*SamplesPerFamily;
struct Key {unsigned eye{},pass{},subview{},kind{};std::array<uint64_t,3> shaders{};unsigned path{};bool operator==(const Key&)const=default;};
struct Family{Key key;unsigned seen{},selected{};};
struct State {
 std::vector<Family> families;unsigned eyeFamilies[Eyes]{},eyeSamples[Eyes]{},pathFamilies[Eyes][2]{},filtered{},overflows{};
 void Reset(){families.clear();memset(pathFamilies,0,sizeof(pathFamilies));eyeFamilies[0]=eyeFamilies[1]=eyeSamples[0]=eyeSamples[1]=filtered=overflows=0;families.reserve(Eyes*FamiliesPerEye);}
 bool Select(const Key& key){
  if(key.eye<1 || key.eye>2 || key.path>1){++filtered;return false;}auto eye=key.eye-1;Family* found{};
  for(auto& family:families)if(family.key==key){found=&family;break;}
  if(!found){if(pathFamilies[eye][key.path]==(key.path?DirectFamilies:WorkerFamilies)){++overflows;return false;}families.push_back({key});found=&families.back();++eyeFamilies[eye];++pathFamilies[eye][key.path];}
  auto n=++found->seen;if(found->selected==SamplesPerFamily || (n>2 && n!=128 && n!=256)){++filtered;return false;}
  ++found->selected;++eyeSamples[eye];return true;
 }
};
}
