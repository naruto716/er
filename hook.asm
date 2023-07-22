.radix 16

extern MainProc:proc
extern BulletCreationWrapper:proc
comm CamAddr:qword
comm TargetAddr:qword
comm TargetCall:qword

.code
public globalHook
public CamHook
public LockOnHook
public BulletCreationHook

globalHook:
push rax
sub rsp,20h
call MainProc
add rsp,20h
pop rax
mov dword ptr [rax+1Ch],00000000h
mov dword ptr [rax+18h],0FFFFFFFFh
ret

CamHook:
movaps [rbx+00000090h],xmm7
lea rdi,[rbx+000000A0h]
mov [CamAddr],rdi
ret

LockOnHook:
sub rsp,28
call TargetCall
mov [TargetAddr],rax
mov rcx,[rax+08]
mov [r13+000006A8],rcx
add rsp,28
ret

BulletCreationHook:
sub rsp,28
mov r8,rbx
lea rdx,[rsp+70]
mov rcx,rsi
call BulletCreationWrapper
add rsp,28
ret


end