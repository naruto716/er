#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3d11on12.h>
#include <d2d1.h>
#include "common.h"

EXTERN_C int _fltused;int _fltused;

HMODULE GameD3D11,GameD2D;unsigned OffsetOfCMDQueueInSwapChain;
PBYTE GameD3D12,GameDXGI;ID3D12CommandQueueVtbl*GameCommandQueueVTable;IDXGISwapChainVtbl*GameSwapChainVTable;
void(STDMETHODCALLTYPE*GameExecuteCommandLists)(ID3D12CommandQueue*,UINT NumCommandLists,ID3D12CommandList*const*ppCommandLists);
HRESULT(STDMETHODCALLTYPE*GamePresent)(IDXGISwapChain*,UINT SyncInterval,UINT Flags);

HWND OverlayWindow;char OverlayMode;
ID3D12CommandQueue*d3d12cmdque;
ID3D11On12Device*d3d11on12;ID3D11Device*d3d11device;ID3D11DeviceContext*d3d11devctx;
ID2D1Factory*d2dfactory;
static bool InitDirect2D()
{
	if(!d2dfactory){
		if(!GameD2D)GameD2D=LoadLibraryExW(L"d2d1.dll",0,LOAD_LIBRARY_SEARCH_SYSTEM32);
		if(GameD2D){
			static HRESULT(WINAPI*D2D1CreateFactory)(D2D1_FACTORY_TYPE,REFIID,const D2D1_FACTORY_OPTIONS*,void**ppIFactory);
			if(!D2D1CreateFactory)(FARPROC&)D2D1CreateFactory=GetProcAddress(GameD2D,"D2D1CreateFactory");
			if(D2D1CreateFactory){
				static D2D1_FACTORY_OPTIONS options={D2D1_DEBUG_LEVEL_NONE};
				D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,IID_ID2D1Factory,&options,(void**)&d2dfactory);
			}
		}
	}
	return d2dfactory;
}

bool InitD3D11On12(ID3D12CommandQueue*cmdque)
{
	if(!cmdque)return false;
	ID3D12Device*d3d12device;
	if(!d3d11device&&cmdque->GetDevice(IID_ID3D12Device,(void**)&d3d12device)==0){
		if(!GameD3D11)GameD3D11=LoadLibraryExW(L"d3d11.dll",0,LOAD_LIBRARY_SEARCH_SYSTEM32);
		if(GameD3D11){
			static PFN_D3D11ON12_CREATE_DEVICE D3D11On12CreateDevice;
			if(!D3D11On12CreateDevice)(FARPROC&)D3D11On12CreateDevice=GetProcAddress(GameD3D11,"D3D11On12CreateDevice");
			if(D3D11On12CreateDevice){
				if(D3D11On12CreateDevice(d3d12device,D3D11_CREATE_DEVICE_SINGLETHREADED|D3D11_CREATE_DEVICE_BGRA_SUPPORT,0,0,(IUnknown**)&cmdque,1,0,&d3d11device,&d3d11devctx,0)==0)
					d3d11device->QueryInterface(IID_ID3D11On12Device,(void**)&d3d11on12);
			}
		}
		d3d12device->Release();
	}
	return d3d11device&&d3d11devctx&&d3d11on12;
}

static ID3D12CommandQueue*CommandQueueFromSwapChain(IDXGISwapChain*swapchain)
{
	if(OffsetOfCMDQueueInSwapChain==0)return 0;
	if(!IsGoodPtr((void**)swapchain+OffsetOfCMDQueueInSwapChain,sizeof(void*)))return 0;
	void**p=(void**)OffsetOfCMDQueueInSwapChain[(void**)swapchain];
	if(!IsGoodPtr(p,sizeof(void*)))return 0;
	return*p==GameCommandQueueVTable?(ID3D12CommandQueue*)p:0;
}

void ExecuteCommandListsHook(ID3D12CommandQueue*This,UINT NumCommandLists,ID3D12CommandList *const *ppCommandLists)
{
	d3d12cmdque=This;
	return GameExecuteCommandLists(This,NumCommandLists,ppCommandLists);
}
static WNDPROC OldWindowProc;
static LRESULT CALLBACK OverlayWindowProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch(uMsg){
	case WM_ERASEBKGND:return 1;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc=BeginPaint(hwnd,&ps);
		if(auto rt=(ID2D1DCRenderTarget*)GetWindowLongPtrW(hwnd,GWLP_USERDATA)){
			RECT rect;GetClientRect(hwnd,&rect);
			rt->BindDC(hdc,&rect);
			rt->BeginDraw();
			static D2D1_COLOR_F black;
			rt->Clear(&black);
			DrawOverlay(rt);
			rt->EndDraw();
		}
		EndPaint(hwnd,&ps);
	}
	return 0;
	case WM_SIZE:
	return 0;
	case WM_DESTROY:if(auto rt=(ID2D1RenderTarget*)GetWindowLongPtrW(hwnd,GWLP_USERDATA))rt->Release();
	break;
	}
	return OldWindowProc(hwnd,uMsg,wParam,lParam);
}

static void InitOverlayWindow(IDXGISwapChain*swapchain)
{
	if(!InitDirect2D())return;
	DXGI_SWAP_CHAIN_DESC desc;WINDOWINFO winfo;
	if(swapchain->GetDesc(&desc)||!desc.Windowed||!desc.OutputWindow||!GetWindowInfo(desc.OutputWindow,&winfo))return;
	OverlayWindow=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_LAYERED,L"Static",0,WS_VISIBLE|WS_POPUP,winfo.rcClient.left,winfo.rcClient.top,winfo.rcClient.right-winfo.rcClient.left,winfo.rcClient.bottom-winfo.rcClient.top,desc.OutputWindow,0,0,0);
	if(!OverlayWindow)return;
	static D2D1_RENDER_TARGET_PROPERTIES rtprop={.type=D2D1_RENDER_TARGET_TYPE_DEFAULT,.pixelFormat={DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE},.dpiX=0,.dpiY=0,
					.usage=D2D1_RENDER_TARGET_USAGE_NONE,.minLevel=D2D1_FEATURE_LEVEL_DEFAULT};
	ID2D1DCRenderTarget*rt;
	if(d2dfactory->CreateDCRenderTarget(&rtprop,&rt)==0){
		rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		SetWindowLongPtrW(OverlayWindow,GWLP_USERDATA,(INT_PTR)rt);
	}
	OldWindowProc=(WNDPROC)SetWindowLongPtrW(OverlayWindow,GWLP_WNDPROC,(INT_PTR)OverlayWindowProc);
	SetLayeredWindowAttributes(OverlayWindow,0,255,LWA_COLORKEY|LWA_ALPHA);
}

static BOOL CALLBACK EnumThreadWndProc(HWND hwnd,LPARAM lParam)
{
	MEMORY_BASIC_INFORMATION mbi;
	if(VirtualQueryEx((HANDLE)-1,(void*)GetWindowLongPtrW(hwnd,GWLP_WNDPROC),&mbi,sizeof(MEMORY_BASIC_INFORMATION))&&mbi.AllocationBase==(void*)lParam)DestroyWindow(hwnd);
	return 1;
}


typedef struct _RTCACHE{
	ID2D1RenderTarget*rt;ID3D11Resource*wrapres;ID3D12Resource*bbuf;
}RTCACHE,*PRTCACHE;
static PRTCACHE GetCachedRenderTarget(IDXGISwapChain*swapchain,unsigned BufferIndex)
{
	ID3D12Resource*bbuf;PRTCACHE result=0;
	if(swapchain->GetBuffer(BufferIndex,IID_ID3D12Resource,(void**)&bbuf)==0){
		static unsigned BufferCount;
		static PRTCACHE rtcache=0;
		if(rtcache==0||BufferIndex>=BufferCount){
			BufferCount=BufferIndex+1;
			rtcache=rtcache?(PRTCACHE)HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,rtcache,BufferCount*sizeof(RTCACHE)):(PRTCACHE)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,BufferCount*sizeof(RTCACHE));
		}
		if(rtcache[BufferIndex].rt==0||rtcache[BufferIndex].wrapres==0||rtcache[BufferIndex].bbuf!=bbuf){
			if(rtcache[BufferIndex].rt)rtcache[BufferIndex].rt->Release(),rtcache[BufferIndex].rt=0;
			if(rtcache[BufferIndex].wrapres)rtcache[BufferIndex].wrapres->Release(),rtcache[BufferIndex].wrapres=0;
			rtcache[BufferIndex].bbuf=0;
			static D3D11_RESOURCE_FLAGS resflags={D3D11_BIND_RENDER_TARGET};
			HRESULT hr=d3d11on12->CreateWrappedResource(bbuf,&resflags,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT,IID_ID3D11Resource,(void**)&rtcache[BufferIndex].wrapres);
			if(hr==0){
				IDXGISurface*dxgisurface;
				if((hr=rtcache[BufferIndex].wrapres->QueryInterface(IID_IDXGISurface,(void**)&dxgisurface))==0){
					static D2D1_RENDER_TARGET_PROPERTIES prop={.type=D2D1_RENDER_TARGET_TYPE_DEFAULT,.pixelFormat={DXGI_FORMAT_UNKNOWN,D2D1_ALPHA_MODE_IGNORE},.dpiX=0,.dpiY=0,
					.usage=D2D1_RENDER_TARGET_USAGE_NONE,.minLevel=D2D1_FEATURE_LEVEL_DEFAULT};
					hr=d2dfactory->CreateDxgiSurfaceRenderTarget(dxgisurface,prop,&rtcache[BufferIndex].rt);
					static bool b;
					if(hr&&!b){
						b=true;
						DXGI_SWAP_CHAIN_DESC desc;
						if(swapchain->GetDesc(&desc))desc.OutputWindow=0;
						MessageBoxW(desc.OutputWindow,L"CreateDxgiSurfaceRenderTarget failed.",0,MB_ICONERROR);
					}
					dxgisurface->Release();
				}
				if(hr){
					rtcache[BufferIndex].rt=0;
					rtcache[BufferIndex].wrapres->Release();
					rtcache[BufferIndex].wrapres=0;
				}else{
					rtcache[BufferIndex].rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
					rtcache[BufferIndex].bbuf=bbuf;
				}
			}
		}
		result=rtcache+BufferIndex;
		bbuf->Release();
	}
	return result;
}

HRESULT PresentHook(IDXGISwapChain*swapchain,UINT SyncInterval,UINT Flags)
{
	if(OverlayWindow){
		DXGI_SWAP_CHAIN_DESC desc;WINDOWINFO winfo;RECT rect;
		if(NeedDrawOverlay()&&swapchain->GetDesc(&desc)==0&&GetWindowInfo(desc.OutputWindow,&winfo)&&GetWindowRect(OverlayWindow,&rect)&&RtlCompareMemory(&rect,&winfo.rcClient,sizeof(RECT))<sizeof(RECT)){
			SetWindowPos(OverlayWindow,HWND_TOP,winfo.rcClient.left,winfo.rcClient.top,winfo.rcClient.right-winfo.rcClient.left,winfo.rcClient.bottom-winfo.rcClient.top,
			SWP_NOACTIVATE|SWP_DEFERERASE|SWP_NOOWNERZORDER|SWP_NOSENDCHANGING|SWP_NOREDRAW|SWP_NOZORDER);
		}
		InvalidateRect(OverlayWindow,0,0);
		MSG msg;
		while(PeekMessageW(&msg,OverlayWindow,0,0,PM_REMOVE))DispatchMessage(&msg);
	}else if(NeedDrawOverlay()){
		HRESULT hr;
		IDXGISwapChain3*swapchain3;unsigned BufferIndex;
		if((hr=swapchain->QueryInterface(IID_IDXGISwapChain3,(void**)&swapchain3))==0){
			BufferIndex=swapchain3->GetCurrentBackBufferIndex();
			swapchain3->Release();
		}else{
			DXGI_SWAP_CHAIN_DESC desc;UINT c;
			if((hr=swapchain->GetDesc(&desc))==0&&(hr=swapchain->GetLastPresentCount(&c))==0)BufferIndex=c%desc.BufferCount;
		}
		ID3D12Resource*bbuf;
		if(OverlayMode)hr=1;
		if(hr==0&&InitD3D11On12(d3d12cmdque)&&InitDirect2D()){
			PRTCACHE rtc=GetCachedRenderTarget(swapchain,BufferIndex);
			if(rtc&&rtc->rt&&rtc->wrapres){
				d3d11on12->AcquireWrappedResources(&rtc->wrapres,1);
				rtc->rt->BeginDraw();
				DrawOverlay(rtc->rt);
				hr=rtc->rt->EndDraw();
				d3d11on12->ReleaseWrappedResources(&rtc->wrapres,1);
				d3d11devctx->Flush();
				if(hr==D2DERR_RECREATE_TARGET){
					hr=0;
					rtc->rt->Release();
					rtc->rt=0;
					rtc->wrapres->Release();
					rtc->wrapres=0;
				}
			}else hr=1;
		}else if(d3d12cmdque&&hr==0)hr=1;
		if(hr){
			static unsigned char failaccum;
			if(++failaccum>16)InitOverlayWindow(swapchain);
		}
	}
	if(void*p=InterlockedExchangePointer(&OldInjectBase,0)){
		EnumThreadWindows(GetCurrentThreadId(),EnumThreadWndProc,(LPARAM)p);
		VirtualFreeEx((HANDLE)-1,p,0,MEM_RELEASE);
	}
	EmitBFJ(GetLockOnFunc,GetLockOnHook,AX);
	return GamePresent(swapchain,SyncInterval,Flags);
}

void GetD3DVTables(HMODULE*d3d12,ID3D12CommandQueueVtbl**CommandQueueVtbl,HMODULE*dxgi,IDXGISwapChainVtbl**SwapChainVtbl)
{
	*dxgi=*d3d12=0;*SwapChainVtbl=0;*CommandQueueVtbl=0;
	ID3D12Device*dev;
	
	PFN_D3D12_CREATE_DEVICE D3D12CreateDevice=0;decltype(::CreateDXGIFactory)*CreateDXGIFactory=0;
	if(HMODULE hModule=LoadLibraryExW(L"d3d12.dll",0,LOAD_LIBRARY_SEARCH_SYSTEM32))
		D3D12CreateDevice=(PFN_D3D12_CREATE_DEVICE)GetProcAddress(hModule,"D3D12CreateDevice");
	if(HMODULE hModule=LoadLibraryExW(L"dxgi.dll",0,LOAD_LIBRARY_SEARCH_SYSTEM32))
		CreateDXGIFactory=(decltype(::CreateDXGIFactory)*)GetProcAddress(hModule,"CreateDXGIFactory");
	if(!D3D12CreateDevice||!CreateDXGIFactory)return;
	HRESULT hr=D3D12CreateDevice(0,D3D_FEATURE_LEVEL_11_0,IID_ID3D12Device,(void**)&dev);
	if(hr)return;
	static D3D12_COMMAND_QUEUE_DESC cqdesc={D3D12_COMMAND_LIST_TYPE_DIRECT,D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,D3D12_COMMAND_QUEUE_FLAG_NONE,0};
	ID3D12CommandQueue*cmdque;
	hr=dev->CreateCommandQueue(&cqdesc,IID_ID3D12CommandQueue,(void**)&cmdque);
	*CommandQueueVtbl=*(ID3D12CommandQueueVtbl**)cmdque;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,*(PCWSTR*)cmdque,d3d12);
	dev->Release();
	if(hr)return;
	IDXGIFactory*dxgifac;
	hr=CreateDXGIFactory(IID_IDXGIFactory,(void**)&dxgifac);
	if(hr==0){
		IDXGISwapChain*swapchain;
		static DXGI_SWAP_CHAIN_DESC sd={{0,0,{1,1},DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,DXGI_MODE_SCALING_UNSPECIFIED},{1,0},
		DXGI_USAGE_RENDER_TARGET_OUTPUT,2,0,1,DXGI_SWAP_EFFECT_FLIP_DISCARD,0};
		sd.OutputWindow=CreateWindowExW(0,L"Static",0,0,0,0,0,0,0,0,0,0);
		hr=dxgifac->CreateSwapChain(cmdque,&sd,&swapchain);
		for(unsigned i=0;i<64&&!OffsetOfCMDQueueInSwapChain;++i)if(i[(void**)swapchain]==cmdque)OffsetOfCMDQueueInSwapChain=i;
		dxgifac->Release();
		if(hr==0){
			*SwapChainVtbl=*(IDXGISwapChainVtbl**)swapchain;
			GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,*(PCWSTR*)swapchain,dxgi);
			swapchain->Release();
		}
		DestroyWindow(sd.OutputWindow);
	}
	cmdque->Release();
}