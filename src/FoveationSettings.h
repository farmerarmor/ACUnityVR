#pragma once
#include <windows.h>
#include <filesystem>
#include <cmath>
#include <cwchar>
namespace FoveationSettings {
struct Settings {bool enabled=true,geometry=true,hidden=false;float radius=.35f;unsigned strength=2,margin=64;};
inline float Number(const std::filesystem::path& path,const wchar_t* key,float fallback,float minimum,float maximum){
 wchar_t text[80]{};GetPrivateProfileStringW(L"Foveation",key,L"",text,_countof(text),path.c_str());if(!text[0])return fallback;
 wchar_t* end{};double value=wcstod(text,&end);while(end && (*end==L' ' || *end==L'\t'))++end;
 return end && end!=text && !*end && std::isfinite(value) && value>=minimum && value<=maximum?float(value):fallback;
}
inline bool Flag(const std::filesystem::path& path,const wchar_t* key,bool fallback){auto value=Number(path,key,float(fallback),0,1);return value==0?false:value==1?true:fallback;}
inline Settings Load(const std::filesystem::path& path){Settings s;
 s.enabled=Flag(path,L"Enabled",true);s.geometry=Flag(path,L"Geometry",true);s.hidden=Flag(path,L"HiddenAreaMask",false);
 s.radius=Number(path,L"CentreRadius",.35f,.1f,1.f);auto strength=Number(path,L"Strength",2,0,3);s.strength=std::floor(strength)==strength?unsigned(strength):2;
 auto margin=Number(path,L"HiddenAreaMargin",64,0,256);s.margin=std::floor(margin)==margin?unsigned(margin):64;return s;
}
}
