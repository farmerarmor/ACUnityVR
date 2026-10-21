; Private DLL ABI shim only. No game executable/shared table patch.
; Save native nonvolatile vectors with complete Win64 unwind metadata.
; The C++ observer may replace ONLY the saved XMM12 low float after validating
; exact native fe9780 caller/current stack and fresh measured headset optics.
; Every other lane/register and original native RAX are preserved.
EXTERN NpcCone63Observer:PROC
PUBLIC NpcCone63Thunk
.code
NpcCone63Thunk PROC FRAME
 sub rsp, 98h
 .allocstack 98h
 movaps [rsp+40h], xmm8
 .savexmm128 xmm8, 40h
 movaps [rsp+50h], xmm12
 .savexmm128 xmm12, 50h
 movaps [rsp+60h], xmm13
 .savexmm128 xmm13, 60h
 .endprolog
 lea rax, [rsp+40h]
 mov [rsp+20h], rax
 mov rax, [rsp+98h]
 mov [rsp+28h], rax
 call NpcCone63Observer
 movaps xmm13, [rsp+60h]
 movaps xmm12, [rsp+50h]
 movaps xmm8, [rsp+40h]
 add rsp, 98h
 ret
NpcCone63Thunk ENDP
END
