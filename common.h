EXTERN_C int(*__imp__snprintf)(char *buffer,size_t count,const char *format,...);
#define _snprintf __imp__snprintf
#define dbgprint(format, ...) \
    do {char s##__LINE__[256];\
        _snprintf(s##__LINE__,256,format,##__VA_ARGS__); \
        OutputDebugStringA(s##__LINE__); \
    } while(0)
#define msgbox(format, ...) \
    do {char s##__LINE__[256];\
        _snprintf(s##__LINE__,256,format,##__VA_ARGS__); \
        MessageBoxA(0,s##__LINE__,0,0); \
    } while(0)
#ifdef __cplusplus
	#define call(f,...) (((char*(*)(...))(void*)(f))(__VA_ARGS__))
#else
	#define call(f,...) (((char*(*)())(void*)(f))(__VA_ARGS__))
#endif
#ifdef __d3d12_h__
typedef struct ID3D12CommandQueueVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ID3D12CommandQueue * This,
            REFIID riid,
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ID3D12CommandQueue * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ID3D12CommandQueue * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetPrivateData )( 
            ID3D12CommandQueue * This,
            _In_  REFGUID guid,
            _Inout_  UINT *pDataSize,
            _Out_writes_bytes_opt_( *pDataSize )  void *pData);
        
        HRESULT ( STDMETHODCALLTYPE *SetPrivateData )( 
            ID3D12CommandQueue * This,
            _In_  REFGUID guid,
            _In_  UINT DataSize,
            _In_reads_bytes_opt_( DataSize )  const void *pData);
        
        HRESULT ( STDMETHODCALLTYPE *SetPrivateDataInterface )( 
            ID3D12CommandQueue * This,
            _In_  REFGUID guid,
            _In_opt_  const IUnknown *pData);
        
        HRESULT ( STDMETHODCALLTYPE *SetName )( 
            ID3D12CommandQueue * This,
            _In_z_  LPCWSTR Name);
        
        HRESULT ( STDMETHODCALLTYPE *GetDevice )( 
            ID3D12CommandQueue * This,
            REFIID riid,
            _COM_Outptr_opt_  void **ppvDevice);
        
        void ( STDMETHODCALLTYPE *UpdateTileMappings )( 
            ID3D12CommandQueue * This,
            _In_  ID3D12Resource *pResource,
            UINT NumResourceRegions,
            _In_reads_opt_(NumResourceRegions)  const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates,
            _In_reads_opt_(NumResourceRegions)  const D3D12_TILE_REGION_SIZE *pResourceRegionSizes,
            _In_opt_  ID3D12Heap *pHeap,
            UINT NumRanges,
            _In_reads_opt_(NumRanges)  const D3D12_TILE_RANGE_FLAGS *pRangeFlags,
            _In_reads_opt_(NumRanges)  const UINT *pHeapRangeStartOffsets,
            _In_reads_opt_(NumRanges)  const UINT *pRangeTileCounts,
            D3D12_TILE_MAPPING_FLAGS Flags);
        
        void ( STDMETHODCALLTYPE *CopyTileMappings )( 
            ID3D12CommandQueue * This,
            _In_  ID3D12Resource *pDstResource,
            _In_  const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate,
            _In_  ID3D12Resource *pSrcResource,
            _In_  const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate,
            _In_  const D3D12_TILE_REGION_SIZE *pRegionSize,
            D3D12_TILE_MAPPING_FLAGS Flags);
        
        void ( STDMETHODCALLTYPE *ExecuteCommandLists )( 
            ID3D12CommandQueue * This,
            _In_  UINT NumCommandLists,
            _In_reads_(NumCommandLists)  ID3D12CommandList *const *ppCommandLists);
        
        void ( STDMETHODCALLTYPE *SetMarker )( 
            ID3D12CommandQueue * This,
            UINT Metadata,
            _In_reads_bytes_opt_(Size)  const void *pData,
            UINT Size);
        
        void ( STDMETHODCALLTYPE *BeginEvent )( 
            ID3D12CommandQueue * This,
            UINT Metadata,
            _In_reads_bytes_opt_(Size)  const void *pData,
            UINT Size);
        
        void ( STDMETHODCALLTYPE *EndEvent )( 
            ID3D12CommandQueue * This);
        
        HRESULT ( STDMETHODCALLTYPE *Signal )( 
            ID3D12CommandQueue * This,
            ID3D12Fence *pFence,
            UINT64 Value);
        
        HRESULT ( STDMETHODCALLTYPE *Wait )( 
            ID3D12CommandQueue * This,
            ID3D12Fence *pFence,
            UINT64 Value);
        
        HRESULT ( STDMETHODCALLTYPE *GetTimestampFrequency )( 
            ID3D12CommandQueue * This,
            _Out_  UINT64 *pFrequency);
        
        HRESULT ( STDMETHODCALLTYPE *GetClockCalibration )( 
            ID3D12CommandQueue * This,
            _Out_  UINT64 *pGpuTimestamp,
            _Out_  UINT64 *pCpuTimestamp);
        
        D3D12_COMMAND_QUEUE_DESC ( STDMETHODCALLTYPE *GetDesc )( 
            ID3D12CommandQueue * This);
        
        END_INTERFACE
    } ID3D12CommandQueueVtbl;
#endif
#ifdef __dxgi_h__
typedef struct IDXGISwapChainVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IDXGISwapChain * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IDXGISwapChain * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IDXGISwapChain * This);
        
        HRESULT ( STDMETHODCALLTYPE *SetPrivateData )( 
            IDXGISwapChain * This,
            /* [annotation][in] */ 
            _In_  REFGUID Name,
            /* [in] */ UINT DataSize,
            /* [annotation][in] */ 
            _In_reads_bytes_(DataSize)  const void *pData);
        
        HRESULT ( STDMETHODCALLTYPE *SetPrivateDataInterface )( 
            IDXGISwapChain * This,
            /* [annotation][in] */ 
            _In_  REFGUID Name,
            /* [annotation][in] */ 
            _In_opt_  const IUnknown *pUnknown);
        
        HRESULT ( STDMETHODCALLTYPE *GetPrivateData )( 
            IDXGISwapChain * This,
            /* [annotation][in] */ 
            _In_  REFGUID Name,
            /* [annotation][out][in] */ 
            _Inout_  UINT *pDataSize,
            /* [annotation][out] */ 
            _Out_writes_bytes_(*pDataSize)  void *pData);
        
        HRESULT ( STDMETHODCALLTYPE *GetParent )( 
            IDXGISwapChain * This,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][retval][out] */ 
            _COM_Outptr_  void **ppParent);
        
        HRESULT ( STDMETHODCALLTYPE *GetDevice )( 
            IDXGISwapChain * This,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][retval][out] */ 
            _COM_Outptr_  void **ppDevice);
        
        HRESULT ( STDMETHODCALLTYPE *Present )( 
            IDXGISwapChain * This,
            /* [in] */ UINT SyncInterval,
            /* [in] */ UINT Flags);
        
        HRESULT ( STDMETHODCALLTYPE *GetBuffer )( 
            IDXGISwapChain * This,
            /* [in] */ UINT Buffer,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][out][in] */ 
            _COM_Outptr_  void **ppSurface);
        
        HRESULT ( STDMETHODCALLTYPE *SetFullscreenState )( 
            IDXGISwapChain * This,
            /* [in] */ BOOL Fullscreen,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pTarget);
        
        HRESULT ( STDMETHODCALLTYPE *GetFullscreenState )( 
            IDXGISwapChain * This,
            /* [annotation][out] */ 
            _Out_opt_  BOOL *pFullscreen,
            /* [annotation][out] */ 
            _COM_Outptr_opt_result_maybenull_  IDXGIOutput **ppTarget);
        
        HRESULT ( STDMETHODCALLTYPE *GetDesc )( 
            IDXGISwapChain * This,
            /* [annotation][out] */ 
            _Out_  DXGI_SWAP_CHAIN_DESC *pDesc);
        
        HRESULT ( STDMETHODCALLTYPE *ResizeBuffers )( 
            IDXGISwapChain * This,
            /* [in] */ UINT BufferCount,
            /* [in] */ UINT Width,
            /* [in] */ UINT Height,
            /* [in] */ DXGI_FORMAT NewFormat,
            /* [in] */ UINT SwapChainFlags);
        
        HRESULT ( STDMETHODCALLTYPE *ResizeTarget )( 
            IDXGISwapChain * This,
            /* [annotation][in] */ 
            _In_  const DXGI_MODE_DESC *pNewTargetParameters);
        
        HRESULT ( STDMETHODCALLTYPE *GetContainingOutput )( 
            IDXGISwapChain * This,
            /* [annotation][out] */ 
            _COM_Outptr_  IDXGIOutput **ppOutput);
        
        HRESULT ( STDMETHODCALLTYPE *GetFrameStatistics )( 
            IDXGISwapChain * This,
            /* [annotation][out] */ 
            _Out_  DXGI_FRAME_STATISTICS *pStats);
        
        HRESULT ( STDMETHODCALLTYPE *GetLastPresentCount )( 
            IDXGISwapChain * This,
            /* [annotation][out] */ 
            _Out_  UINT *pLastPresentCount);
        
        END_INTERFACE
    } IDXGISwapChainVtbl;
void GetD3DVTables(HMODULE*d3d12,ID3D12CommandQueueVtbl**CommandQueueVtbl,HMODULE*dxgi,IDXGISwapChainVtbl**SwapChainVtbl);
extern void(STDMETHODCALLTYPE*GameExecuteCommandLists)(ID3D12CommandQueue*,UINT NumCommandLists,ID3D12CommandList*const*ppCommandLists);
extern ID3D12CommandQueueVtbl*GameCommandQueueVTable;extern IDXGISwapChainVtbl*GameSwapChainVTable;
extern HRESULT(STDMETHODCALLTYPE*GamePresent)(IDXGISwapChain*This,UINT SyncInterval,UINT Flags);
extern void ExecuteCommandListsHook(ID3D12CommandQueue*,UINT NumCommandLists,ID3D12CommandList *const *ppCommandLists);
extern HRESULT PresentHook(IDXGISwapChain*,UINT SyncInterval,UINT Flags);
#endif
bool IsGoodPtr(const void*p,unsigned len,unsigned align=0);
extern HANDLE Heap;extern PBYTE GameEXEBase,InjectBase,*LockOnChrPtr,*LockOnUIPtr,GetLockOnFunc,GameD3D12,GameDXGI;extern void*OldInjectBase;
EXTERN_C char*LockOnTarget,GetLockOnHook[];EXTERN_C void*GetLockOnHookReturn;extern char OverlayMode;
extern double(*wtf)(const wchar_t*);
extern decltype(bsearch)*pbsearch;
extern const char*GetNPCName(int id);
extern void RelocateNPCNames(ptrdiff_t diff);
extern bool NeedDrawOverlay();
extern bool OverlayFixed,OverlayShowName,OverlayShowHP,OverlayShowMP,OverlayShowSP,OverlayShowPoise,OverlayShowPoison,OverlayShowRot,OverlayShowBleed,OverlayShowBlight,OverlayShowFrost,OverlayShowSleep,OverlayShowMad;
extern wchar_t OverlayFontName[64];extern float OverlayFontSize,OverlayDX,OverlayDY,OverlayCX,OverlayCY;
#ifdef _D2D1_H_
extern void DrawOverlay(ID2D1RenderTarget*rt);
#endif
EXTERN_C void*(*__imp_RtlMoveMemory)(void*Destination,const void*Source,SIZE_T Length);
#define AX 0
#define CX 1
#define DX 2
#define BX 3
#define SP 4
#define BP 5
#define SI 6
#define DI 7
void EmitBFJ(void*from,void*to,char reg);