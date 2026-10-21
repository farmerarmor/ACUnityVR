#pragma once
// A normal D3D dispatch implementation can call its instrumented variant.
// Run correction scopes only around the outer call, forward each native once.
namespace DispatchRoute76 {
template<class Function> class Scope {
 inline static thread_local unsigned depth{};inline static thread_local Function current{};
 Function previous{};bool outer{};
public:
 explicit Scope(Function native):previous(current),outer(depth++==0){if(outer)current=native;}
 ~Scope(){--depth;current=previous;}
 bool Outermost()const{return outer;}
 static Function Current(){return current;}
 Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
};
}
