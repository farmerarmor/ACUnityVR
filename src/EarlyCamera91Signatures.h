// Exact native camera update, dispatch, resolver, lookup and full47 descriptor.
static bool Guard91(const uint8_t* image){
 const struct{size_t rva,size;uint64_t hash;} ranges[]={
  {0x1f3f190,0xd5d,0xb53b687275e44db7ULL},
  {0x1f5f990,0xe,0x3d398319e239ae8fULL},
  {0x1cd0d30,0x16,0x0a23f47aa64346a4ULL},
  {0x20b2240,0x45,0xf1a64e9443001727ULL},
  {0x1ef7860,0x48,0x7bdd1e5e008f8932ULL},
  {0x1ef75f0,0x4c,0xadd57482a04b03e6ULL},
  {0x1ef7d20,0xbe,0xc93cfb242c9dc453ULL},
  {0x4eec760,0x5e0,0xa870bd45cf589b66ULL},
 };
 for(auto& r:ranges){uint64_t h=14695981039346656037ULL;for(size_t i=0;i<r.size;++i)h=(h^image[r.rva+i])*1099511628211ULL;if(h!=r.hash)return false;}
 return true;
}
