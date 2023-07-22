#pragma once
EXTERN_C int(*__imp__snprintf)(char* buffer, size_t count, const char* format, ...);
EXTERN_C int(*__imp_swprintf_s)(wchar_t* buffer,size_t sizeOfBuffer,const wchar_t* format,...);

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

#define AX 0
#define CX 1
#define DX 2
#define BX 3
#define SP 4
#define BP 5
#define SI 6
#define DI 7

//bullet
#define BNORMAL 0
#define BSELF 1
#define BLIGHTNINGSTORM 2
#define LIGHTNINGTELEPORT 3
#define BLIGHTNINGSTORM_SELF 4
#define SHADOWTELEPORT 5
#define BGRAVITY 6
#define DOANIM 7
#define CHRTURNTO 8
#define DARKREALM 9
#define LIGHTNINGFIST 10
#define SETANIMSPEED 11
#define ESP 12
#define BEAM 13
#define FROSTREALM 14
#define WEAPONBUFF1 15
#define SWORDSUMMON 16
#define AQUARIUM 17
#define POOPSTORM 18
#define SWORDSUMMON_SPHERE 19
#define SWORDARRAY 20
#define SWORDARRAY_2 21
#define WEAPONBUFF2 22
#define NOBULLET 23
//bullet end