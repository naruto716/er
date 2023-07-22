#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include <d3d12.h>
#include <d2d1.h>
#include <dwrite.h>
#include "common.h"

float VSCR_W=1920.f,VSCR_H=1080.f;
bool OverlayFixed,OverlayShowName,OverlayShowHP,OverlayShowMP,OverlayShowSP,OverlayShowPoise,OverlayShowPoison,OverlayShowRot,OverlayShowBleed,OverlayShowBlight,OverlayShowFrost,OverlayShowSleep,OverlayShowMad;
wchar_t OverlayFontName[64];float OverlayFontSize,OverlayDX,OverlayDY,OverlayCX,OverlayCY;

static IDWriteFactory*DWriteFactory;
static bool InitDWrite()
{
	if(!DWriteFactory){
		static const GUID IID_IDWriteFactory={0xb859ee5a, 0xd838, 0x4b5b,{0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48}};
		HMODULE dwrite=LoadLibraryExW(L"dwrite.dll",0,LOAD_LIBRARY_SEARCH_SYSTEM32);
		static HRESULT(*DWriteCreateFactory)(DWRITE_FACTORY_TYPE factoryType,REFIID iid,IDWriteFactory**factory);
		if(!DWriteCreateFactory)(FARPROC&)DWriteCreateFactory=GetProcAddress(dwrite,"DWriteCreateFactory");
		if(DWriteCreateFactory)DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,IID_IDWriteFactory,&DWriteFactory);
	}
	if(!DWriteFactory)return false;
	return true;
}

static void ScaleRect(ID2D1RenderTarget*rt,D2D_RECT_F&rect)
{
	D2D_SIZE_F rtsize=rt->GetSize();
	rect.left*=rtsize.width/VSCR_W;
	rect.top*=rtsize.height/VSCR_H;
	rect.right*=rtsize.width/VSCR_W;
	rect.bottom*=rtsize.height/VSCR_H;
}

static bool ResetTextFormat(ID2D1RenderTarget*rt,IDWriteTextFormat*&txtfmt)
{
	static D2D_SIZE_F rtsize1;bool result=false;
	D2D_SIZE_F rtsize=rt->GetSize();
	if(txtfmt&&rtsize.height!=rtsize1.height){
		txtfmt->Release();
		txtfmt=0;
	}
	if(!txtfmt){
		DWriteFactory->CreateTextFormat(OverlayFontName,0,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,OverlayFontSize*rtsize.height/VSCR_H,L"en-us",&txtfmt);
		result=true;
	}
	rtsize1=rtsize;
	return result;
}

typedef struct _TEXTLAYOUT_CACHE{
	IDWriteTextLayout*layout;
	float size,width,height;
	unsigned short len;
	wchar_t text[64];
}TEXTLAYOUT_CACHE,*PTEXTLAYOUT_CACHE;

static IDWriteTextLayout*GetCachedTextLayout(ID2D1RenderTarget*rt,IDWriteTextFormat*txtfmt,const wchar_t*text,unsigned len,float width,float height,unsigned index)
{
	static TEXTLAYOUT_CACHE cache[24];
	if(index>=_countof(cache)||len>_countof(cache->text))return 0;
	if(cache[index].layout&&(cache[index].len!=len||RtlCompareMemory(cache[index].text,text,len*sizeof(wchar_t))<len*sizeof(wchar_t)||cache[index].size!=txtfmt->GetFontSize())){
		cache[index].layout->Release();
		cache[index].layout=0;
	}
	if(!cache[index].layout){
		D2D_SIZE_F rtsize=rt->GetSize();
		if(DWriteFactory->CreateTextLayout(text,len,txtfmt,width,height,&cache[index].layout)==0){
			__imp_RtlMoveMemory(cache[index].text,text,len*sizeof(wchar_t));
			cache[index].len=len;
			cache[index].size=txtfmt->GetFontSize();
			cache[index].width=width;
			cache[index].height=height;
		}
	}
	return cache[index].layout;
}

static void DrawStatBar(ID2D1RenderTarget*rt,ID2D1SolidColorBrush*brush,const D2D1_COLOR_F&color,IDWriteTextFormat*txtfmt,int voff,const wchar_t*title,int val,int maxval,bool flt=false)
{
	float LockOnPos[2];
	if(OverlayFixed){
		LockOnPos[0]=VSCR_W/2;
		LockOnPos[1]=VSCR_H/2;
	}else{
		LockOnPos[0]=0[(float*)(0x54B0+*LockOnUIPtr)];
		LockOnPos[1]=1[(float*)(0x54B0+*LockOnUIPtr)];
	}
	brush->SetColor(color);
	D2D_SIZE_F rtsize=rt->GetSize();
	D2D_RECT_F rect={LockOnPos[0]+OverlayDX-OverlayCX-OverlayFontSize,LockOnPos[1]+OverlayDY+voff*OverlayCY,LockOnPos[0]+OverlayDX-OverlayCX/2-OverlayFontSize/2,LockOnPos[1]+OverlayDY+OverlayCY+voff*OverlayCY};
	ScaleRect(rt,rect);
	txtfmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
	if(auto layout=GetCachedTextLayout(rt,txtfmt,title,lstrlenW(title),rect.right-rect.left,rect.bottom-rect.top,voff*2))
		rt->DrawTextLayout((D2D_POINT_2F&)rect,layout,brush,D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
	
	wchar_t s[64];
	unsigned n=flt?wsprintfW(s,L" %d/%d",(int)(float&)val,(int)(float&)maxval):wsprintfW(s,L" %u/%u",val,maxval);
	
	rect.left=(LockOnPos[0]+OverlayDX-OverlayCX/2)*rtsize.width/VSCR_W;
	
	if(flt){
		if((float&)maxval<1)(float&)maxval=1;
		if((float&)val>(float&)maxval)(float&)val=(float&)maxval;
		if((float&)val<0)(float&)val=0;
		rect.right=rect.left+OverlayCX*(float&)val/(float&)maxval*rtsize.width/VSCR_W;
	}else{
		if(maxval<1)maxval=1;
		if(val>maxval)val=maxval;
		if(val<0)val=0;
		rect.right=rect.left+OverlayCX*(float)val/(float)maxval*rtsize.width/VSCR_W;
	}
	
	rect.top+=OverlayCY/4*rtsize.height/VSCR_H;
	rect.bottom-=OverlayCY/4*rtsize.height/VSCR_H;
	rt->FillRectangle(rect,brush);
	rect.right=(LockOnPos[0]+OverlayDX+OverlayCX/2)*rtsize.width/VSCR_W;
	rt->DrawRectangle(rect,brush,1,0);
	
	rect.left=rect.right;
	rect.right=rtsize.width;
	rect.top-=OverlayCY/4*rtsize.height/VSCR_H;
	rect.bottom+=OverlayCY/4*rtsize.height/VSCR_H;
	txtfmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
	
	if(auto layout=GetCachedTextLayout(rt,txtfmt,s,n,rect.right-rect.left,rect.bottom-rect.top,voff*2+1))
		rt->DrawTextLayout((D2D_POINT_2F&)rect,layout,brush,D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
}

bool NeedDrawOverlay()
{
	return IsGoodPtr(0x54C4+*LockOnUIPtr,2)&&*(short*)(0x54C4+*LockOnUIPtr);
}

void DrawOverlay(ID2D1RenderTarget*rt)
{
	if(!NeedDrawOverlay())return;
	if(IsGoodPtr(LockOnTarget,sizeof(void*),sizeof(void*)))if(InitDWrite()){
		
		char*stats=*(char**)(LockOnTarget+0x190);
		if(!IsGoodPtr(stats,sizeof(void*),sizeof(void*)))return;
		char*poise=*(char**)(stats+0x40);
		char*resist=*(char**)(stats+0x20);
		stats=*(char**)stats;
		if(!IsGoodPtr(stats,1)||!IsGoodPtr(poise,1)||!IsGoodPtr(resist,1))return;
		static IDWriteTextFormat*txtfmt;
		ResetTextFormat(rt,txtfmt);
		
		static D2D1_COLOR_F white={1,1,1,1},red={1,.2,.2,1},green={0,1,0,1},gray={0.7,.7,.7,1},blue={.2,.3,1,1},darkgreen={0,.5,.1,1},orange={.75,.5,0,1},darkred={.75,0,0,1},
		black={0,0,0,1},lightblue={.2,.6,.9,1},purple={.5,.4,.6,1},yellow={.8,.8,0,1};
		float LockOnPos[2];
		if(OverlayFixed){
			LockOnPos[0]=VSCR_W/2;
			LockOnPos[1]=VSCR_H/2;
		}else{
			LockOnPos[0]=0[(float*)(0x54B0+*LockOnUIPtr)];
			LockOnPos[1]=1[(float*)(0x54B0+*LockOnUIPtr)];
		}
		ID2D1SolidColorBrush*brush;static D2D1_BRUSH_PROPERTIES bprop={1,{1,0,0,1,0,0}};
		if(rt->CreateSolidColorBrush(&white,&bprop,&brush)==0){
			D2D_RECT_F rect={LockOnPos[0]+OverlayDX-OverlayCX/2,LockOnPos[1]+OverlayDY,LockOnPos[0]+OverlayDX+OverlayCX/2,LockOnPos[1]+OverlayDY+OverlayCY};
			ScaleRect(rt,rect);
			if(OverlayShowName){
				int ParamID=*(int*)(LockOnTarget+0x60);static int LastParamID;static const char*ls;static wchar_t ws[64];static int n;
				
				if(ParamID!=LastParamID){
					const char*s=GetNPCName(ParamID);
					if(s!=ls){
						n=MultiByteToWideChar(CP_UTF8,0,s,-1,ws,_countof(ws));
						ls=s;
					}
					LastParamID=ParamID;
				}
				
				if(n>1){
					txtfmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
					if(auto layout=GetCachedTextLayout(rt,txtfmt,ws,n,rect.right-rect.left,rect.bottom-rect.top,0))
						rt->DrawTextLayout((D2D_POINT_2F&)rect,layout,brush,D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
				}
			}
			int voff=0;
			if(OverlayShowHP)DrawStatBar(rt,brush,red,txtfmt,++voff,L"HP",*(int*)(stats+0x138),*(int*)(stats+0x13C));
			if(OverlayShowMP)DrawStatBar(rt,brush,blue,txtfmt,++voff,L"MP",*(int*)(stats+0x148),*(int*)(stats+0x14C));
			if(OverlayShowSP)DrawStatBar(rt,brush,green,txtfmt,++voff,L"SP",*(int*)(stats+0x154),*(int*)(stats+0x158));
			
			if(OverlayShowPoise)DrawStatBar(rt,brush,gray,txtfmt,++voff,L"Poise",*(int*)(poise+16),*(int*)(poise+20),true);
			
			if(OverlayShowPoison)DrawStatBar(rt,brush,darkgreen,txtfmt,++voff,L"Poison",*(int*)(resist+0x10),*(int*)(resist+0x2C));
			if(OverlayShowRot)DrawStatBar(rt,brush,orange,txtfmt,++voff,L"Rot",*(int*)(resist+0x14),*(int*)(resist+0x30));
			if(OverlayShowBleed)DrawStatBar(rt,brush,darkred,txtfmt,++voff,L"Bleed",*(int*)(resist+0x18),*(int*)(resist+0x34));
			if(OverlayShowBlight)DrawStatBar(rt,brush,black,txtfmt,++voff,L"Blight",*(int*)(resist+0x1C),*(int*)(resist+0x38));
			if(OverlayShowFrost)DrawStatBar(rt,brush,lightblue,txtfmt,++voff,L"Frost",*(int*)(resist+0x20),*(int*)(resist+0x3C));
			if(OverlayShowSleep)DrawStatBar(rt,brush,purple,txtfmt,++voff,L"Sleep",*(int*)(resist+0x24),*(int*)(resist+0x40));
			if(OverlayShowMad)DrawStatBar(rt,brush,yellow,txtfmt,++voff,L"Mad",*(int*)(resist+0x28),*(int*)(resist+0x44));
			brush->Release();
		}
	}
}