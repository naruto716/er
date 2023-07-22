#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include <psapi.h>
#include <dxgi.h>
#include <d3d12.h>
#include "common.h"
HMODULE msvcrt;double(*wtf)(const wchar_t*);
HANDLE Heap;PBYTE GameEXEBase,InjectBase,*LockOnChrPtr,*LockOnUIPtr,GetLockOnFunc;void*OldInjectBase;
static unsigned FindGameProcess()
{
	HWND hwnd=FindWindowW(L"ELDEN RING™",L"ELDEN RING™");DWORD pid=0;
	if(hwnd)GetWindowThreadProcessId(hwnd,&pid);
	return pid;
}

bool IsGoodPtr(const void*p,unsigned len,unsigned align)
{
	if(align>1&&(UINT_PTR)p%align)return false;
	static SYSTEM_INFO SystemInfo;
	if(!SystemInfo.lpMaximumApplicationAddress)GetSystemInfo(&SystemInfo);
	return (UINT_PTR)SystemInfo.lpMinimumApplicationAddress<=(UINT_PTR)p&&(UINT_PTR)p+len<=(UINT_PTR)SystemInfo.lpMaximumApplicationAddress&&!IsBadReadPtr(p,len);
}

static void StartGameProcess(LPPROCESS_INFORMATION pi)
{
	static STARTUPINFOW si={sizeof(STARTUPINFOW)};
	SetEnvironmentVariableW(L"SteamAppId",L"1245620");
	SetEnvironmentVariableW(L"SteamGameId",L"1245620");
	wchar_t FileName[MAX_PATH];
	DWORD n=GetModuleFileNameW(0,FileName,_countof(FileName));
	while(n&&FileName[n]!=L'\\')--n;
	FileName[n]=0;
	SetCurrentDirectoryW(FileName);
	lstrcpyW(FileName+n,L"\\eldenring.exe");
	if(!CreateProcessW(FileName,0,0,0,0,0,0,0,&si,pi))pi->dwProcessId=0;
	if(!pi->dwProcessId){
		FileName[n]=0;
		lstrcpyW(FileName+n,L"\\Game");
		SetCurrentDirectoryW(FileName);
		lstrcpyW(FileName+n,L"\\Game\\eldenring.exe");
		if(!CreateProcessW(FileName,0,0,0,0,0,0,0,&si,pi))pi->dwProcessId=0;
	}
}

static PBYTE FindModule(HANDLE hProcess,const wchar_t*FileName)
{
	DWORD cbNeeded=0,cbNeeded1;HMODULE Result;
	EnumProcessModules(hProcess,&Result,sizeof(HMODULE),&cbNeeded);
	if(FileName==0)return (PBYTE)Result;
	HMODULE*hModules=cbNeeded?(HMODULE*)HeapAlloc(Heap,0,cbNeeded):0;
	if(!hModules)return 0;
	Result=0;
	EnumProcessModules(hProcess,hModules,cbNeeded,&cbNeeded1);
	wchar_t s[MAX_PATH];
	for(unsigned i=0;i<cbNeeded/sizeof(HMODULE)&&i<cbNeeded1/sizeof(HMODULE)&&!Result;++i)
		if(GetModuleFileNameExW(hProcess,hModules[i],s,_countof(s))&&lstrcmpiW(s,FileName)==0)
			Result=hModules[i];
	HeapFree(Heap,0,hModules);
	return (PBYTE)Result;
}

static bool SetPointer(HANDLE hProcess,void*pp,void*p)
{
	DWORD OldProtect;BOOL ret;
	BOOL proted=VirtualProtectEx(hProcess,pp,sizeof(void*),PAGE_EXECUTE_READWRITE,&OldProtect);
	ret=WriteProcessMemory(hProcess,pp,&p,sizeof(void*),0);
	if(proted)VirtualProtectEx(hProcess,pp,sizeof(void*),OldProtect,&OldProtect);
	return ret;
}

void EmitBFJ(void*from,void*to,char reg)
{
	unsigned char code[13],len;
	if(reg<8){
		code[0]=0x48;code[1]=0xb8+reg;code[10]=0xff;code[11]=0xe0+reg;
		len=12;
	}else if(reg<16){
		code[0]=0x49;code[1]=0xb0+reg;code[10]=0x41;code[11]=0xff;code[12]=0xd8+reg;
		len=13;
	}else return;
	*(void**)(code+2)=to;
	if(RtlCompareMemory(to,code,len)==len)return;
	DWORD OldProtect;
	if(VirtualProtectEx((HANDLE)-1,from,len,PAGE_EXECUTE_READWRITE,&OldProtect)){
		__imp_RtlMoveMemory(from,code,len);
		VirtualProtectEx((HANDLE)-1,from,len,OldProtect,&OldProtect);
	}
}

PBYTE AOBScanModule(HANDLE hProcess,const BYTE*Pattern,unsigned PatternLength,void*BaseAddress,DWORD Protect)
{
	MEMORY_BASIC_INFORMATION mbi;PBYTE Result=0;
	if(hProcess==0)hProcess=(HANDLE)-1;
	for(mbi.BaseAddress=BaseAddress;!Result&&VirtualQueryEx(hProcess,mbi.BaseAddress,&mbi,sizeof(MEMORY_BASIC_INFORMATION))&&mbi.AllocationBase==BaseAddress;(UINT_PTR&)mbi.BaseAddress+=mbi.RegionSize)
	if(mbi.Protect&Protect)
	if(PBYTE buf=hProcess!=(HANDLE)-1?(PBYTE)VirtualAllocEx((HANDLE)-1,0,mbi.RegionSize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE):(PBYTE)mbi.BaseAddress){
		if(hProcess==(HANDLE)-1||ReadProcessMemory(hProcess,mbi.BaseAddress,buf,mbi.RegionSize,0)){
			unsigned i;
			for(i=0;i+PatternLength<mbi.RegionSize&&RtlCompareMemory(Pattern,buf+i,PatternLength)<PatternLength;++i);
			if(i+PatternLength<mbi.RegionSize)Result=(PBYTE)mbi.BaseAddress+i;
		}
		if(hProcess!=(HANDLE)-1)VirtualFreeEx((HANDLE)-1,buf,0,MEM_RELEASE);
	}
	return Result;
}

bool IsInModule(HANDLE hProcess,const void*p,const void*mod)
{
	MEMORY_BASIC_INFORMATION mbi;
	return VirtualQueryEx(hProcess,p,&mbi,sizeof(MEMORY_BASIC_INFORMATION))&&mbi.AllocationBase==mod;
}


void LoadINI()
{
	wchar_t FileName[MAX_PATH];
	DWORD n=GetModuleFileNameW(0,FileName,_countof(FileName)-4),dot=n;
	while(dot&&FileName[dot]!=L'.')--dot;
	if(dot==0)dot=n;
	FileName[dot]=L'.';
	FileName[dot+1]=L'i';
	FileName[dot+2]=L'n';
	FileName[dot+3]=L'i';
	FileName[dot+4]=0;
	OverlayMode=GetPrivateProfileIntW(L"OVERLAY",L"Mode",0,FileName);
	OverlayFixed=GetPrivateProfileIntW(L"OVERLAY",L"Fixed",0,FileName);
	GetPrivateProfileStringW(L"OVERLAY",L"DX",L"0",OverlayFontName,_countof(OverlayFontName),FileName);
	OverlayDX=wtf(OverlayFontName);
	GetPrivateProfileStringW(L"OVERLAY",L"DY",L"32",OverlayFontName,_countof(OverlayFontName),FileName);
	OverlayDY=wtf(OverlayFontName);
	GetPrivateProfileStringW(L"OVERLAY",L"CX",L"128",OverlayFontName,_countof(OverlayFontName),FileName);
	OverlayCX=wtf(OverlayFontName);
	GetPrivateProfileStringW(L"OVERLAY",L"CY",L"12",OverlayFontName,_countof(OverlayFontName),FileName);
	OverlayCY=wtf(OverlayFontName);
	GetPrivateProfileStringW(L"OVERLAY",L"FontSize",L"11",OverlayFontName,_countof(OverlayFontName),FileName);
	OverlayFontSize=wtf(OverlayFontName);
	GetPrivateProfileStringW(L"OVERLAY",L"FontName",L"Consolas",OverlayFontName,_countof(OverlayFontName),FileName);
	
	OverlayShowName=GetPrivateProfileIntW(L"OVERLAY",L"ShowName",1,FileName);
	OverlayShowHP=GetPrivateProfileIntW(L"OVERLAY",L"ShowHP",1,FileName);
	OverlayShowMP=GetPrivateProfileIntW(L"OVERLAY",L"ShowMP",1,FileName);
	OverlayShowSP=GetPrivateProfileIntW(L"OVERLAY",L"ShowSP",1,FileName);
	OverlayShowPoise=GetPrivateProfileIntW(L"OVERLAY",L"ShowPoise",1,FileName);
	OverlayShowPoison=GetPrivateProfileIntW(L"OVERLAY",L"ShowPoison",1,FileName);
	OverlayShowRot=GetPrivateProfileIntW(L"OVERLAY",L"ShowRot",1,FileName);
	OverlayShowBleed=GetPrivateProfileIntW(L"OVERLAY",L"ShowBleed",1,FileName);
	OverlayShowBlight=GetPrivateProfileIntW(L"OVERLAY",L"ShowBlight",1,FileName);
	OverlayShowFrost=GetPrivateProfileIntW(L"OVERLAY",L"ShowFrost",1,FileName);
	OverlayShowSleep=GetPrivateProfileIntW(L"OVERLAY",L"ShowSleep",1,FileName);
	OverlayShowMad=GetPrivateProfileIntW(L"OVERLAY",L"ShowMad",1,FileName);
}

EXTERN_C void start()
{
	HMODULE SelfBase=GetModuleHandleW(0);
	{
		wchar_t s[64];
		wsprintfW(s,L"%x%x%x",*(unsigned*)((char*)SelfBase+0x138),(UINT_PTR)GetModuleHandleW,(UINT_PTR)FindWindowW);
		if(HANDLE hMutex=CreateMutexExW(0,s,0,SYNCHRONIZE))
			if(GetLastError()==ERROR_ALREADY_EXISTS)ExitProcess(0);
	}
	Heap=GetProcessHeap();
	msvcrt=LoadLibraryExW(L"msvcrt.dll",0,LOAD_LIBRARY_SEARCH_SYSTEM32);
	(FARPROC&)wtf=GetProcAddress(msvcrt,"_wtof");
	(FARPROC&)pbsearch=GetProcAddress(msvcrt,"bsearch");
	
	
	PROCESS_INFORMATION pi;
	pi.hProcess=0;
	if(pi.dwProcessId=FindGameProcess()){
		pi.hProcess=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_READ|PROCESS_VM_WRITE|SYNCHRONIZE,0,pi.dwProcessId);
	}else{
		StartGameProcess(&pi);
	}
	if(!pi.hProcess){
winerror:wchar_t s[64];
		if(FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM,0,GetLastError(),0,s,_countof(s),0))MessageBoxW(0,s,0,MB_ICONERROR);
		ExitProcess(0);
	}
	HMODULE d3d12,dxgi;
	ID3D12CommandQueueVtbl*cmdquevtbl;IDXGISwapChainVtbl*swapchainvtbl;
	GetD3DVTables(&d3d12,&cmdquevtbl,&dxgi,&swapchainvtbl);
	if(!d3d12||!cmdquevtbl||!dxgi||!swapchainvtbl){
		MessageBoxW(0,L"can't load d3d",0,MB_ICONERROR);
		ExitProcess(0);
	}
	LoadINI();
	WaitForInputIdle(pi.hProcess,INFINITE);
	GameEXEBase=FindModule(pi.hProcess,0);
	if(!GameEXEBase){
		MessageBoxW(0,L"can't find game exe base address.",0,MB_ICONERROR);
		ExitProcess(0);
	}
	PBYTE LocalGameEXE=0;
	{
		wchar_t s[MAX_PATH];DWORD n=_countof(s);
		if(QueryFullProcessImageNameW(pi.hProcess,0,s,&n)){
			LocalGameEXE=(PBYTE)LoadLibraryExW(s,0,LOAD_LIBRARY_AS_IMAGE_RESOURCE);
			(UINT_PTR&)LocalGameEXE&=~255;
		}
	}
	if(!LocalGameEXE)goto winerror;
	static const BYTE LockOnUIAOB[]={0x0F,0xB6,0xC8,0x48,0x39,0xB2,0x88,0x02,0x00,0x00,0x0F,0x45,0xCE,0x88,0x4D,0x67,0x48,0x8B,0x0D};
	static const BYTE LockOnChrAOB[]={0x48,0x89,0x44,0x24,0x50,0x48,0x8B,0xD9,0xC7,0x44,0x24,0x24,0x00,0x00,0x00,0x00,0x48,0x8B,0x0D};
	static const BYTE GetLockOnFuncAOB[]={0x49,0x8B,0x06,0x49,0x8B,0xCE,0xFF,0x90,0x78,0x01,0x00,0x00,0x48,0x85,0xC0,0x74,0x20,0x48,0x8B,0x00};
	PBYTE p=AOBScanModule(0,LockOnUIAOB,sizeof LockOnUIAOB,LocalGameEXE,-1);
	if(!p){
aoberror:MessageBoxW(0,L"aob error",0,MB_ICONERROR);
		ExitProcess(0);
	}
	(PBYTE&)LockOnUIPtr=p+23+*(int*)(p+19)-LocalGameEXE+GameEXEBase;
	if(!IsInModule(pi.hProcess,LockOnUIPtr,GameEXEBase))goto aoberror;
	//c7 83 ?? ?? 00 00 01 00 00 00 F3 0F 2C 83 ?? ?? 00 00   54B0=screen x 54C4=lockon
	
	p=AOBScanModule(0,LockOnChrAOB,sizeof LockOnChrAOB,LocalGameEXE,-1);
	if(!p)goto aoberror;
	(PBYTE&)LockOnChrPtr=p+23+*(int*)(p+19)-LocalGameEXE+GameEXEBase;
	if(!IsInModule(pi.hProcess,LockOnChrPtr,GameEXEBase))goto aoberror;
	
	GetLockOnFunc=AOBScanModule(0,GetLockOnFuncAOB,sizeof GetLockOnFuncAOB,LocalGameEXE,-1);
	if(!GetLockOnFunc)goto aoberror;
	GetLockOnFunc=GetLockOnFunc-LocalGameEXE+GameEXEBase;
	GetLockOnHookReturn=GetLockOnFunc+15;
	for(unsigned i=0;i<64&&(!GameD3D12||!GameDXGI);++i){
		wchar_t s[MAX_PATH];
		if(!GameD3D12)GameD3D12=GetModuleFileNameW(d3d12,s,_countof(s))?FindModule(pi.hProcess,s):0;
		if(!GameDXGI)GameDXGI=GetModuleFileNameW(dxgi,s,_countof(s))?FindModule(pi.hProcess,s):0;
		if(WaitForSingleObject(pi.hProcess,256)==0)ExitProcess(0);
	}
	if(!GameD3D12||!GameDXGI){
		MessageBoxW(0,L"can't locate game d3d dll",0,MB_ICONERROR);
		ExitProcess(0);
	}
	(PBYTE&)GameCommandQueueVTable=((UINT_PTR)cmdquevtbl-(UINT_PTR)d3d12+GameD3D12);
	(PBYTE&)GameExecuteCommandLists=((UINT_PTR)cmdquevtbl->ExecuteCommandLists-(UINT_PTR)d3d12+GameD3D12);
	(PBYTE&)GameSwapChainVTable=((UINT_PTR)swapchainvtbl-(UINT_PTR)dxgi+GameDXGI);
	(PBYTE&)GamePresent=((UINT_PTR)swapchainvtbl->Present-(UINT_PTR)dxgi+GameDXGI);
	
	p=0;MODULEINFO modinfo;
	if(ReadProcessMemory(pi.hProcess,&GameSwapChainVTable->Present,&p,sizeof(void*),0)&&GetModuleInformation(pi.hProcess,(HMODULE)GameDXGI,&modinfo,sizeof(MODULEINFO))&&(p<GameDXGI||p>GameDXGI+modinfo.SizeOfImage)){
		MEMORY_BASIC_INFORMATION mbi;
		if(VirtualQueryEx(pi.hProcess,p,&mbi,sizeof(MEMORY_BASIC_INFORMATION))&&mbi.State==MEM_COMMIT&&mbi.Type==MEM_PRIVATE&&mbi.AllocationProtect==PAGE_EXECUTE_READWRITE&&mbi.AllocationProtect==mbi.Protect)
			OldInjectBase=(PBYTE)mbi.AllocationBase;
	}
	
	if(wchar_t*cmdline=GetCommandLineW()){
		if(*cmdline++==L'\"')while(*cmdline&&*cmdline!=L'\"')++cmdline;
		while(*cmdline&&*cmdline!=L' ')++cmdline;
		while(*cmdline&&*cmdline==L' ')++cmdline;
		if(lstrcmpiW(cmdline,L"unload")==0){
			SetPointer(pi.hProcess,&GameCommandQueueVTable->ExecuteCommandLists,GameExecuteCommandLists);
			SetPointer(pi.hProcess,&GameSwapChainVTable->Present,GamePresent);
			WriteProcessMemory(pi.hProcess,GetLockOnFunc,GetLockOnFuncAOB,sizeof GetLockOnFuncAOB,0);
			if(OldInjectBase){
				WaitForSingleObject(pi.hProcess,1024);
				VirtualFreeEx(pi.hProcess,OldInjectBase,0,MEM_RELEASE);
			}
			ExitProcess(0);
		}
	}
	if(!GetModuleInformation((HANDLE)-1,SelfBase,&modinfo,sizeof(MODULEINFO)))goto winerror;
	InjectBase=(PBYTE)VirtualAllocEx(pi.hProcess,0,modinfo.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
	RelocateNPCNames(InjectBase-(PBYTE)SelfBase);
	if(!InjectBase||!WriteProcessMemory(pi.hProcess,InjectBase,SelfBase,modinfo.SizeOfImage,0))goto winerror;
	if(!SetPointer(pi.hProcess,&GameCommandQueueVTable->ExecuteCommandLists,(UINT_PTR)ExecuteCommandListsHook-(UINT_PTR)SelfBase+InjectBase)||
	   !SetPointer(pi.hProcess,&GameSwapChainVTable->Present,(UINT_PTR)PresentHook-(UINT_PTR)SelfBase+InjectBase))goto winerror;
	ExitProcess(0);
}