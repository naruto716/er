#define _USE_MATH_DEFINES

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include <psapi.h>
#include <math.h>
#include "header.h"
#include "drv.h"
EXTERN_C NTSYSAPI ULONG NTAPI RtlRandomEx(PULONG);
wchar_t INIPath[MAX_PATH];
EXTERN_C char globalHook[];
EXTERN_C char CamHook[];
EXTERN_C float* CamAddr;
EXTERN_C char LockOnHook[];
EXTERN_C char* TargetAddr;
EXTERN_C char BulletCreationHook[];
ULONG Seed;
PBYTE WindowProc, BulletBase, BulletFunc, BulletMan_accessor, WorldChrMan, CamBase, ParamBase, EffectFunc, BulletCreationInject, BulletCreationFunc;
HWND GameHwnd;
float espDist;
bool isEspOn;
HANDLE hDevice;

template<class ResultType, class PointerType>
ResultType* AccessMultilevelPointer(PointerType BaseAddress)
{
    return (intptr_t)BaseAddress < 65536 || IsBadReadPtr((void*)(intptr_t)BaseAddress, sizeof(ResultType)) ? 0 : (ResultType*)BaseAddress;
}

template<class ResultType, class PointerType, class OffsetType, class...OffsetTypes>
ResultType* AccessMultilevelPointer(PointerType BaseAddress, OffsetType Offset, OffsetTypes... Offsets)
{
    return (intptr_t)BaseAddress < 65536 || (intptr_t)BaseAddress & (sizeof(void*) - 1) || IsBadReadPtr((void*)(intptr_t)BaseAddress, sizeof(void*)) ? 0 : AccessMultilevelPointer<ResultType>(*(char**)(intptr_t)BaseAddress + Offset, Offsets...);
}

static unsigned GetGameProcessID() {
    GameHwnd = FindWindowW(L"ELDEN RING™", L"ELDEN RING™");
    DWORD pid;
    return GameHwnd && GetWindowThreadProcessId(GameHwnd, &pid) ? pid : 0;
}

/*
static HMODULE GetModule(HANDLE pHandle) {
    HMODULE hModule = 0;
    DWORD junk;
    EnumProcessModules(pHandle, &hModule, sizeof hModule, &junk);
    return hModule;
}
*/

static HMODULE GetModuleAddress(HANDLE pHandle, const wchar_t* NameToFind, unsigned CountOfName) {
    SYSTEM_INFO SysInfo;
    GetSystemInfo(&SysInfo);
    MEMORY_BASIC_INFORMATION mbi;
    for (char* p = (char*)SysInfo.lpMinimumApplicationAddress;
        VirtualQueryEx(pHandle, p, &mbi, sizeof mbi) && p <= SysInfo.lpMaximumApplicationAddress;
        p = (char*)mbi.BaseAddress + mbi.RegionSize)
    {
        if (mbi.State == MEM_COMMIT && mbi.Type == MEM_IMAGE && mbi.BaseAddress == mbi.AllocationBase)
        {
            wchar_t path[MAX_PATH];
            unsigned length = GetMappedFileNameW(pHandle, mbi.AllocationBase, path, _countof(path));
            if (length > CountOfName) {
                if (_wcsnicmp(path + length - CountOfName, NameToFind, CountOfName) == 0) {
                    //MessageBoxW(0, path, 0, 0);
                    return (HMODULE)mbi.AllocationBase;
                }
            }
        }
    }
    return 0;
}

PBYTE MapSection(HANDLE hfile, unsigned pid, HANDLE section, unsigned prot) {
    DWORD junk;
    MMAP_PARAMS MapParam;
    MapParam.pid = pid;
    MapParam.prot = PAGE_EXECUTE_READWRITE;
    MapParam.section = section;
    MapParam.addr = 0;
    MapParam.len = 0;
    auto result = DeviceIoControl(hfile, IOCTL_MAPSECTION, &MapParam, sizeof MapParam, &MapParam, sizeof MapParam, &junk, 0);
    //msgbox("MapSection: %d\n Last Error: %d", result, GetLastError());
    return (PBYTE)(result ? MapParam.addr : 0);
}

bool WriteMem(HANDLE hfile, unsigned pid, void* selfAddr, void* targetAddr, size_t nBytes) {
    DWORD junk;
    COPYMEM_PARAMS MemParam;
    MemParam.from = selfAddr;
    MemParam.to = targetAddr;
    MemParam.len = nBytes;
    MemParam.pid = pid;
    auto result = DeviceIoControl(hfile, IOCTL_WRITEMEM, &MemParam, sizeof MemParam, 0, 0, &junk, 0);
    //msgbox("WriteMem: %d\n Last Error: %d\n TargetAddr:%p", result, GetLastError(), targetAddr);
    return result;
}

void emitcall(unsigned pid, void* from, void* to, char reg, unsigned len)
{
    unsigned char code[32], n;
    if (reg < 8) {
        code[0] = 0x48; code[1] = 0xb8 + reg; code[10] = 0xff; code[11] = 0xd0 + reg;
        n = 12;
    }
    else if (reg < 16) {
        code[0] = 0x49; code[1] = 0xb0 + reg; code[10] = 0x41; code[11] = 0xff; code[12] = 0xc8 + reg;
        n = 13;
    }
    else return;
    *(void**)(code + 2) = to;
    while (n < len && n < sizeof code)code[n++] = 0x90;
    WriteMem(hDevice, pid, code, from, len);
}

void emitjump(unsigned pid, void* from, void* to, char reg, unsigned len)
{
    unsigned char code[32], n;
    if (reg < 8) {
        code[0] = 0x48; code[1] = 0xb8 + reg; code[10] = 0xff; code[11] = 0xe0 + reg;
        n = 12;
    }
    else if (reg < 16) {
        code[0] = 0x49; code[1] = 0xb0 + reg; code[10] = 0x41; code[11] = 0xff; code[12] = 0xd8 + reg;
        n = 13;
    }
    else return;
    *(void**)(code + 2) = to;
    while (n < len && n < sizeof code)code[n++] = 0x90;
    DWORD OldProtect;
    WriteMem(hDevice, pid, code, from, len);
}

PBYTE GetParamTable(unsigned Index, wchar_t** out = 0) {
    uintptr_t hdr = *(uintptr_t*)(*(uintptr_t*)ParamBase + Index * 72 + 0x88);
    if (!hdr) return 0;
    if (out)
        *out = *(wchar_t**)(hdr + 24);
    return *(BYTE**)(*(uintptr_t*)(hdr + 0x80) + 0x80);
}

PBYTE ParamIDtoAddr(PBYTE TableBase, int id) {
    short n = *(short*)(TableBase + 10);
    for (int i = 0; i < n; ++i) {
        int d = *(int*)(TableBase + 64 + 24 * i);
        if (d == id)
            return TableBase + *(int*)(TableBase + 64 + 24 * i + 8);
    }
    return 0;
}

PINT64 GameProc(PINT64* prcx, PINT64 prdx) {
    auto addr = prcx[2];
    if (addr) {
        //esp
        PBYTE espflag = AccessMultilevelPointer<BYTE>((PBYTE)addr + 0x190, 0x28, 0x1904);
        int* pcurrentAnim = AccessMultilevelPointer<int>((PBYTE)addr + 0x190, 0x18, 0x40);
        if (espflag && pcurrentAnim && *pcurrentAnim != -1) {
            if (isEspOn) {
                float* ptcoord = AccessMultilevelPointer<float>((PBYTE)addr + 0x190, 0x68, 0x54);
                float* p = AccessMultilevelPointer<float>(WorldChrMan, 0x18468, 0x190, 0x68, 0x54);
                float dx = ptcoord[7] - p[7], dy = ptcoord[8] - p[8], dz = ptcoord[9] - p[9];
                float distance = sqrt(dx * dx + dy * dy + dz * dz);
                if (distance < espDist)
                    *espflag = 1;
                else
                    *espflag = 0;
            }
            else
            {
                *espflag = 0;
            }
        }
        //original code
        int64_t addr2 = addr[1];
        *prdx = addr2;
        return prdx;
    }
    *prdx = -1;
    return prdx;
}

//Bullet
inline float RandomFloat(float low, float high) {
    return low + (double)RtlRandomEx(&Seed) / 4294967295.0 * (double)(high - low);
}

inline int RandomInt(int low, int high) {
    return low + RtlRandomEx(&Seed) % (high - low + 1);
}

inline void DoAnim(int AnimID) {
    int* panim = AccessMultilevelPointer<int>(WorldChrMan, 0xB658, 0, 0x190, 0x58, 0x18);
    if (panim)
        *panim = AnimID;
}

void ApplyEffect(int EffectID) {
    PBYTE* pr8 = AccessMultilevelPointer<PBYTE>(WorldChrMan, 0x18468);
    if (!pr8) return;
    PBYTE* prcx = (PBYTE*)( * (PBYTE*)(pr8)+0x178);
    call(EffectFunc, *prcx, EffectID, *pr8, *pr8);
}

inline void SetAnimSpeed(float speed) {
    if (auto p = AccessMultilevelPointer<float>(WorldChrMan, 0x18468, 0x190, 0x28, 0x17C8))
        *p = speed;
}

bool GetTargetCoord(float* TargetCoord, float defaultDist = 10) {
    PBYTE pLockOnState = *(BYTE**)(CamBase)+0x2830;
    if ((intptr_t)pLockOnState > 0xFFFF && *pLockOnState) {
        float* p = AccessMultilevelPointer<float>(TargetAddr + 0x190, 0x68, 0x70);
        TargetCoord[0] = p[0]; TargetCoord[1] = p[1]; TargetCoord[2] = p[2];
        return true;
    }
    if (auto p = AccessMultilevelPointer<float>(WorldChrMan, 0x18468, 0x190, 0x68, 0x54)) {
        auto azimuth = atan2(p[2], p[0]) * 2;
        TargetCoord[0] = p[7] - defaultDist * sin(azimuth); TargetCoord[1] = p[8]; TargetCoord[2] = p[9] + defaultDist * cos(azimuth);
        return false;
    }
    TargetCoord[0] = 0; TargetCoord[0] = 0; TargetCoord[0] = 0;
    return false;
}

bool GetTargetAzimuth(float* Azimuth) {
    PBYTE pLockOnState = *(BYTE**)(CamBase)+0x2830;
    if ((intptr_t)pLockOnState > 0xFFFF && *pLockOnState) {
        float* p = AccessMultilevelPointer<float>(TargetAddr + 0x190, 0x68, 0x54);
        *Azimuth = atan2(p[2], p[0]) * 2;
        return true;
    }
    return false;
}

void RotateCylin(float* pxoff, float* pyoff, float* pzoff, float yaw, float pitch = 0.f) {
    auto x = *pxoff;
    *pxoff = cos(yaw) * x - sin(yaw) * *pzoff;
    *pzoff = sin(yaw) * x + cos(yaw) * *pzoff;
    if (pitch)
        *pyoff = -tan(pitch) * *pyoff;
}

void RotateCylin(float& pxoff, float& pyoff, float& pzoff, float yaw, float pitch = 0.f) {
    auto x = pxoff;
    pxoff = cos(yaw) * x - sin(yaw) * pzoff;
    pzoff = sin(yaw) * x + cos(yaw) * pzoff;
    if (pitch)
        pyoff = -tan(pitch) * pyoff;
}

inline void Normalize(float& dx, float& dy, float& dz) {
    float srt = sqrt(dx * dx + dz * dz);
    dx /= srt; dy /= srt; dz /= srt;
}

void ChrTurnTo(float x, float z) {
    if (auto p = AccessMultilevelPointer<float>(WorldChrMan, 0x18468, 0x190, 0x68, 0x54)) {
        double dx = x - p[7]; double dz = z - p[9];
        double angle = atan2(-dx, dz);
        p[2] = sin(angle/2); p[0] = cos(angle/2);
    }
}

inline int GetPlayerAnim() {
    if (auto panim = AccessMultilevelPointer<int>(WorldChrMan, 0x18468, 0x190, 0x18, 0x40))
        return *panim;
    return 0;
}

struct bullet_t {
    uint64_t starttime = 0;
    int bid = 0;
    unsigned OwnerID = 0;
    int quantity = 0;
    int interval = 0;
    int key = 0;
    int QuantityChange = 0;
    float x = 0, y = 0, z = 0, ax = 0, ay = 0, az = 0;
    int node = 0;
    bool overlap = true;
    bool serverside = true;
    bool autoTarget = true;
    //constructors
    bullet_t() = default;
    bullet_t(int bid, int quantity, uint32_t interval, int key = BSELF, int ownerid = 0x16900000, uint64_t starttime = GetTickCount64(), int qchange = -1,  bool serverside = true, float x = 0, float y = 0, float z = 0, float ax = 0, float ay = 0, float az = 0, int node = 0, bool overlap = true, bool autoTarget = true) : OwnerID(ownerid), bid(bid), quantity(quantity), interval(interval), key(key), starttime(starttime), serverside(serverside), QuantityChange(qchange), x(x), y(y), z(z), ax(ax), ay(ay), az(az), node(node), overlap(overlap), autoTarget(autoTarget) {}
    //operators
    bullet_t& operator+=(const bullet_t& b2) {
        if (key == b2.key && bid == b2.bid) {
            starttime = b2.starttime;
            quantity += b2.quantity;
            serverside = b2.serverside;
            OwnerID = b2.OwnerID;
            interval = b2.interval;
            QuantityChange = b2.QuantityChange;
            x = b2.x; y = b2.y; z = b2.z; ax = b2.ax; ay = b2.ay; az = b2.az;
            node = b2.node;
        }
        else
            *this = b2;
        return *this;
    }
};

bullet_t BulletQueue[1024];

void BulletSpawn(int BulletID, float X, float Y, float Z, float XAngle, float YAngle, float ZAngle, int NodeID = 0, unsigned OwnerID = 0x16900000, int MagicID = 4000, bool Serverside = true, float InitXAngle = 0, float InitYAngle = 0, float InitZAngle = 0, bool autoTarget= true) {
    char junk[256];
    static uint64_t bulletbuffer[] = { 0xFFFFFFFF16900000,0x00000FA0FFFFFFFF,0x009EB10000000000,0x00002774FFFFFFFF,0xFFFFFFFFFFFFFFFF,0xFFFFFFFFFFFFFFFF,0x3F8000003F8B851F,0x3F8000003F898232,0x0000000900000000,0x0000000000000001,0x000000003F10C355,0x00000000BF532395,0x3F80000180000000,0x0000000000000000,0x000000003F532395,0x000000003F10C355,0x40A97E3FC0C8983B,0x3F800000C08EB5B9,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x000000409D0FF0FF,0x00007FF300000000,0xFFFFFFFFFFFFFFFF,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x00000000FFFFFFFF,0x0000000000000000,0x3F800000FFFFFFFF,0x3F8000003F898232,0x00007FF301F78A00,0x00007FF40FEC53E8,0x0000D5786A97B007,0x00007FF70000076C,0x00007FF36476CAD0,0x000000409D0FF120,0x000000409D0FF000,0x00007FF770E4A1AB,0x0000000000000000,0x0000000000000000,0x00007FF36476CAD0,0x000000409D0FF120 };
    char* p = (char*)bulletbuffer;
    if (!InitXAngle)
        InitXAngle = -XAngle;
    if (!InitYAngle)
        InitYAngle = YAngle;
    if (!InitZAngle)
        InitZAngle = ZAngle;
    memcpy(p, &OwnerID, 4);
    memcpy(p + 0xC, &MagicID, 4);
    memcpy(p + 0x14, &BulletID, 4);
    memcpy(p + 0x1C, &NodeID, 4);
    memcpy(p + 0x70, &XAngle, 4);
    memcpy(p + 0x74, &YAngle, 4);
    memcpy(p + 0x78, &ZAngle, 4);
    memcpy(p + 0x80, &X, 4);
    memcpy(p + 0x84, &Y, 4);
    memcpy(p + 0x88, &Z, 4);
    memcpy(p + 0x58, &InitXAngle, 4);//?
    memcpy(p + 0x68, &InitYAngle, 4);//?
    memcpy(p + 0x50, &InitZAngle, 4);//?
    memcpy(p + 0x44, &Serverside, 4);
    //*(int*)(p + 0x44) = 0;
    //*(p + 0x44) |= autoTarget << 4;
    //*(p + 0x44) |= Serverside << 3; Related to spawning point
    *(int64_t*)(p + 0x60) = 0; //InitAngleAdjustment?

    call(BulletFunc, *(uintptr_t*)BulletBase, junk + 0x20, bulletbuffer, junk + 0x40);
}

int AddBullet(const bullet_t& bullet) {
    if (bullet.key != BNORMAL && bullet.overlap == true) {
        for (int i = 0; i < _countof(BulletQueue); ++i) {
            if (BulletQueue[i].key == bullet.key && BulletQueue[i].bid == bullet.bid) {
                BulletQueue[i] += bullet;
                return i;
            }
        }
    }
    for (int i = 0; i < _countof(BulletQueue); ++i) {
        if (BulletQueue[i].quantity == 0) {
            BulletQueue[i] = bullet;
            return i;
        }
    }
    return -1;
}

int AddBullet(int bid, int quantity, uint32_t interval, int key = BSELF, int ownerid = 0x16900000, uint64_t starttime = GetTickCount64(), int qchange = -1, int serverside = 9, float x = 0, float y = 0, float z = 0, float ax = 0, float ay = 0, float az = 0, int node = 0, bool overlap = true, bool autoTarget = true) {
    return AddBullet(bullet_t(bid, quantity, interval, key, ownerid, starttime, qchange, serverside, x, y, z, ax, ay, az, node, overlap, autoTarget));
}

bool RemoveBullet(const bullet_t& bullet) {
    bool result = false;
    for (bullet_t& i : BulletQueue) {
        if (i.key == bullet.key && i.bid == bullet.bid){
            i.key = 0;
            i.quantity = 0;
            result = true;
        }
    }
    return result;
}

bool RemoveBullet(int key) {
    bool result = false;
    for (bullet_t& i : BulletQueue) {
        if (i.key == key) {
            i.key = 0;
            i.quantity = 0;
            result = true;
        }
    }
    return result;
}

bool RemoveBullet(int key, int bid) {
    bool result = false;
    for (bullet_t& i : BulletQueue) {
        if (i.key == key && i.bid == bid) {
            i.key = 0;
            i.quantity = 0;
            result = true;
        }
    }
    return result;
}

void BulletProc() {
    if (auto p = AccessMultilevelPointer<float>(WorldChrMan, 0x18468, 0x190, 0x68, 0x54)) {// in game CamAddr[45] - Yaw CamAddr[46] - Pitch
        int bcountonce = 0;            //float* p = AccessMultilevelPointer<float>(TargetAddr + 0x190, 0x68, 0x70); p[0],p[1],p[2] target coord
        for (bullet_t& i : BulletQueue) {
            if (i.quantity > 0 && i.starttime <= GetTickCount64()) {
                float x = 0, y = 0, z = 0, ax = 0, ay = 0, az = 0;
                int NodeID = 0, OwnerID = 0x16900000, MagicID = 4000, serverside = 9;
                auto angle = atan2(p[2], p[0]) * 2;
                switch (i.key) {
                case BSELF:
                {
                    float xoff = 0, yoff = 0, zoff = 5; //Rotation Test
                    RotateCylin(&xoff, &yoff, &zoff, angle, p[46]);
                    x = p[7] + xoff; y = p[8] + 1; z = p[9] + zoff; ax = -sin(angle); ay = -tan(CamAddr[46]); az = cos(angle);

                    //float p1[3]; //Target Test
                    //GetTargetCoord(p1);
                    //x = p1[0]; y = p1[1]; z = p1[2]; ax = -sin(angle); ay = -tan(CamAddr[46]); az = cos(angle);

                    //x = p[7]; y = p[8]+1; z = p[9]; ax = -sin(angle); ay = -tan(CamAddr[46]); az = cos(angle);
                    BulletSpawn(i.bid, x, y, z, ax, ay, az, 1, OwnerID, MagicID, serverside);
                    break;
                }
                case BLIGHTNINGSTORM://i.x min, i.y max, i.z rate
                {
                    static const int bulletlist[] = { 10694009, 10691010, 10692109, 10504001, 10692009, 10693000 };
                    float angle = RandomFloat(-M_PI, M_PI), radius = RandomFloat(i.x, i.y);
                    float t[3];
                    GetTargetCoord(t);
                    x = t[0] + radius * cos(angle); y = t[1]; z = t[2] + radius * sin(angle);
                    i.bid = bulletlist[RandomInt(0, _countof(bulletlist) - 1)];
                    if (i.z && i.y - i.z > i.x) {
                        i.y -= i.z;
                    }
                    BulletSpawn(i.bid, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);
                    if (i.quantity == 1)
                        BulletSpawn(4650240, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);
                    break;
                }
                case BLIGHTNINGSTORM_SELF:
                {
                    static const int bulletlist[] = { 10694009, 10691010, 10692109, 10504001, 10692009, 10693000 };
                    float angle = RandomFloat(-M_PI, M_PI), radius = RandomFloat(i.x, i.y);
                    x = p[7] + radius * cos(angle); y = p[8]; z = p[9] + radius * sin(angle);
                    i.bid = bulletlist[RandomInt(0, _countof(bulletlist) - 1)];
                    if (i.z && i.y - i.z > i.x) {
                        i.y -= i.z;
                    }
                    BulletSpawn(i.bid, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);
                    if (i.quantity == 1)
                        BulletSpawn(4650240, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);
                    break;
                }
                case LIGHTNINGTELEPORT:
                {
                    float t[3];
                    if (GetTargetCoord(t)) {
                        DoAnim(60500);
                        i.x = 5;
                        AddBullet(5, 20, 50, BLIGHTNINGSTORM_SELF, 378535936, GetTickCount64() + 2000, -1, 9, 0, 5, 0, 0, 0, 0, 0);
                        AddBullet(10501000, 1, 0, BNORMAL, 378535936, GetTickCount64() + 4000, -1, 9, t[0], t[1], t[2], 0, 0, 0, 5);
                        p[7] = t[0]; p[8] = t[1]; p[9] = t[2];
                    }
                    break;
                }
                case SHADOWTELEPORT:
                {
                    float t[3];
                    if (GetTargetCoord(t)) {
                        float tazimuth;
                        GetTargetAzimuth(&tazimuth);
                        p[7] = t[0] + 3 * sin(tazimuth); p[8] = t[1]; p[9] = t[2] - 3 * cos(tazimuth);
                        p[2] = sin(tazimuth/2); p[0] = cos(tazimuth/2);
                        DoAnim(90005);
                    }
                    break;
                }
                case BGRAVITY:
                {
                    float xoff = 0, yoff = 0, zoff = 10; //Rotation Test
                    RotateCylin(&xoff, &yoff, &zoff, angle, p[46]);
                    x = p[7] + xoff; y = p[8] + 2; z = p[9] + zoff; ax = -sin(angle); ay = -tan(CamAddr[46]); az = cos(angle);
                    BulletSpawn(i.bid, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);
                    AddBullet(2200191, 5, 10, BNORMAL, 378535936, GetTickCount64(), -1, 9, x, y, z, ax, ay, az, 0);
                    AddBullet(2200101, 1, 0, BNORMAL, 378535936, GetTickCount64() + 2000, -1, 9, x, y, z, ax, ay, az, 0);
                    AddBullet(10732000, 5, 0, BNORMAL, 378535936, GetTickCount64() + 4800, -1, 9, x, y, z, ax, ay, az, 1);
                    AddBullet(2200372, 1, 0, BNORMAL, 378535936, GetTickCount64() + 5000, -1, 9, x, y - 1, z, ax, ay, az, 0);
                    AddBullet(0, 40, 50, CHRTURNTO, 378535936, GetTickCount64() + 2500, -1, 9, x, y, z);
                    AddBullet(80200, 1, 0, DOANIM, 0, GetTickCount64() + 2350);
                    AddBullet(0, 1, 0, DOANIM, 0, GetTickCount64() + 5000);
                    break;
                }
                case DARKREALM:
                {
                    static const int bulletlist[] = { 10503010, 10503011, 10653000, 10653000, 10653000, 10653000, 10500000, 10500000, 10621000 };
                    float angle = RandomFloat(-M_PI, M_PI), radius = RandomFloat(0, 30);
                    x = p[7] + radius * cos(angle); y = p[8] + 1; z = p[9] + radius * sin(angle);
                    i.bid = bulletlist[RandomInt(0, _countof(bulletlist) - 1)];
                    float aangle = RandomFloat(-M_PI, M_PI);
                    ax = sin(aangle); az = cos(aangle);
                    if (i.bid == 10653000) y += 5;
                    else if (i.bid == 10621000) {
                        y += 7; ax = 0; az = 0; ay = -1;
                        AddBullet(10625002, 1, 0, BNORMAL, 378535936, GetTickCount64() + 500, -1, 9, x, y - 8, z, 0, 0, 0, 0);
                    }
                    else if (i.bid == 10503010 || i.bid == 10503011) {
                        y += 1; ay = tan(RandomFloat(-M_PI / 2, M_PI / 2));
                    }
                    BulletSpawn(i.bid, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);
                    break;
                }
                case FROSTREALM: {
                    static const int bulletlist[] = { 2410, 2410, 2410, 10441000, 2260, 10702000, 10702100 };
                    float angle = RandomFloat(-M_PI, M_PI), radius = RandomFloat(0, 30);
                    x = p[7] + radius * cos(angle); y = p[8] + 1; z = p[9] + radius * sin(angle);
                    i.bid = bulletlist[RandomInt(0, _countof(bulletlist) - 1)];
                    float aangle = RandomFloat(-M_PI, M_PI);
                    ax = sin(aangle); az = cos(aangle);
                    BulletSpawn(i.bid, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);
                    static int count = 0;
                    if (count >= 9) {
                        BulletSpawn(2260, p[7], p[8], p[9], ax, ay, az, NodeID, OwnerID, MagicID, serverside);
                        count = 0;
                    }
                    else
                        ++count;
                    break;
                }
                case LIGHTNINGFIST:
                {
                    if (GetPlayerAnim() == 42030505) {
                        RemoveBullet(LIGHTNINGFIST);
                        break;
                    }
                    float xoff = RandomFloat(-7, 7), yoff = 0, zoff = RandomFloat(2, 7);
                    RotateCylin(&xoff, &yoff, &zoff, angle, 0);
                    x = p[7] + xoff; y = p[8] + yoff + 7; z = p[9] + zoff;
                    float bazimuth = RandomFloat(-M_PI, M_PI);
                    float baltitude = RandomFloat(-M_PI / 2, -M_PI / 6);
                    ax = sin(bazimuth); ay = tan(baltitude); az = cos(bazimuth);
                    if (bazimuth > 0 && bazimuth < M_PI / 12 && i.quantity > 60)
                        BulletSpawn(2200102, x, y - RandomFloat(2, 5), z, 0, 0, 0);
                    BulletSpawn(i.bid, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside, 0, 0, 0, true);
                    break;
                }
                case BEAM:
                {
                    if (GetPlayerAnim() != 407045112) {
                        RemoveBullet(BEAM);
                        break;
                    }
                    float tcoord[3];
                    GetTargetCoord(tcoord,15);
                    float dx = 2, dy = 0, dz = 0;
                    RotateCylin(&dx, &dy, &dz, angle, 0);
                    x = p[7] + dx; y = p[8] + dy + 1; z = p[9] + dz;
                    float x2 = p[7] - dx, y2 = p[8] + dy + 1, z2 = p[9] - dz;
                    ax = tcoord[0] - x; ay = tcoord[1] + 1 - y; az = tcoord[2] - z;
                    float ax2 = tcoord[0] - x2, ay2 = tcoord[1] + 1 - y2, az2 = tcoord[2] - z2;
                    float hypo = sqrt(ax * ax + az * az);
                    ax /= hypo; ay /= hypo; az /= hypo;
                    BulletSpawn(10420000, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside, -ax, ay, az);
                    BulletSpawn(10420002, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside, -ax, ay, az);
                    BulletSpawn(10420000, x2, y2, z2, ax2, ay2, az2, NodeID, OwnerID, MagicID, serverside, -ax2, ay2, az2);
                    BulletSpawn(10420002, x2, y2, z2, ax2, ay2, az2, NodeID, OwnerID, MagicID, serverside, -ax2, ay2, az2);
                    static int subcount = 0;
                    static constexpr int arccount = 20;
                    if (subcount >= 10) {
                        for (int v = 0; v < arccount; ++v) {
                            BulletSpawn(10407000, p[7], p[8] + 0.5, p[9], sin(2*M_PI / arccount * v), ay, cos(2*M_PI / arccount * v), NodeID, OwnerID, MagicID, serverside);
                        }
                        subcount = 0;
                        BulletSpawn(10413003, p[7], p[8], p[9], cos(angle+M_PI/2), ay, sin(angle + M_PI / 2), NodeID, OwnerID, MagicID, serverside);
                    }
                    else
                        ++subcount;
                    break;
                }
                case SWORDSUMMON:
                {
                    float dx = RandomFloat(-7, 7), dy = RandomFloat(1, 8), dz = -2;
                    RotateCylin(dx, dy, dz, angle);
                    x = p[7] + dx; y = p[8] + dy; z = p[9] + dz;
                    float tcoord[3];
                    GetTargetCoord(tcoord,99999.f);
                    ax = tcoord[0] - x; ay = tcoord[1] - y; az = tcoord[2] - z;
                    float hypo = sqrt(ax * ax + az * az);
                    ax /= hypo; ay /= hypo; az /= hypo;
                    static const int swordbulletlist[] = { 10439003, 10430101, 10430001 };
                    BulletSpawn(10420004, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);//ripple1
                    BulletSpawn(10439001, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);//ripple2
                    BulletSpawn(10420000, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);
                    //BulletSpawn(10430201, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);
                    AddBullet(10430201, 1, 0, BNORMAL, 378535936, GetTickCount64() + 200, -1, 9, x, y, z, ax, ay, az);//sword
                    AddBullet(swordbulletlist[RandomInt(0, _countof(swordbulletlist) - 1)], 1, 0, BNORMAL, 378535936, GetTickCount64() + 850, -1, 9, x, y, z, ax, ay, az);
                    break;
                }
                case AQUARIUM:
                {
                    static const int bulletlist[] = { 2690, 10510000, 10511000 };
                    float angle = RandomFloat(-M_PI, M_PI), radius = RandomFloat(0,10);
                    //float t[3];
                    //GetTargetCoord(t);
                    x = p[7]; y = p[8] + 2.5; z = p[9];
                    ax = sin(angle); ay = tan(RandomFloat(0, M_PI/2)); az = cos(angle);
                    i.bid = bulletlist[RandomInt(0, _countof(bulletlist) - 1)];
                    BulletSpawn(i.bid, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);
                    break;
                }
                case POOPSTORM:
                {
                    float angle = RandomFloat(-M_PI, M_PI), radius = RandomFloat(0, 25);
                    float t[3];
                    GetTargetCoord(t);
                    x = t[0] + cos(angle) * radius; y = t[1] + 7; z = t[2] + sin(angle) * radius;
                    ax = 0; ay = -1; az = 0;
                    BulletSpawn(i.bid, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);
                    angle = RandomFloat(-M_PI, M_PI);
                    if (RandomInt(0,100) < 50)
                        BulletSpawn(10722000, x, y - 2, z, cos(angle), 0, sin(angle), NodeID, OwnerID, MagicID, serverside);
                    break;
                }
                case SWORDSUMMON_SPHERE:
                {
                    constexpr float radius = 7;
                    float t[3];
                    GetTargetCoord(t);
                    t[1] += 1;
                    for (int i = 0; i < 60; ++i) {
                        float azimuth = RandomFloat(-M_PI, M_PI);
                        float altitude = RandomFloat(0, M_PI);
                        float dy = radius * sin(altitude);
                        float temp = radius * cos(altitude);
                        float dx = temp * cos(azimuth);
                        float dz = temp * sin(azimuth);
                        x = t[0] + dx; y = t[1] + dy; z = t[2] + dz;
                        ax = -dx; ay = -dy; az = -dz;
                        Normalize(ax, ay, az);
                        BulletSpawn(10439000, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);
                        BulletSpawn(10439002, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);
                    }
                    AddBullet(10436101, 1, 1, BNORMAL, 378535936, GetTickCount64() + 3700, -1, 9, t[0], t[1], t[2]);
                    break;
                }
                case SWORDARRAY:
                {
                    AddBullet(10413003, 6, 500, BNORMAL, 378535936, GetTickCount64(), -1, 9, p[7], p[8], p[9], cos(angle + M_PI / 2), 1, sin(angle + M_PI / 2));
                    int off = 0;
                    x = p[7]; y = p[8]; z = p[9];
                    for (float radius = 5; radius >= 2; radius -= 1.5) {
                        for (float theta = 0; theta < 2 * M_PI; theta += 2 * M_PI / (radius * 3)) {
                            float dx = cos(theta) * radius, dy = 0.5, dz = sin(theta) * radius;
                            AddBullet(10439000, 1, 0, BNORMAL, 378535936, GetTickCount64() + 200 * off, -1, 9, x + dx, y + dy, z + dz, 0, 1, 0);
                            AddBullet(10439002, 1, 0, BNORMAL, 378535936, GetTickCount64() + 200 * off, -1, 9, x + dx, y + dy, z + dz, 0, 1, 0);
                        }
                        ++off;
                    }
                    AddBullet(1, 100, 20, SWORDARRAY_2, 378535936, GetTickCount64() + 5000);
                    break;
                }
                case SWORDARRAY_2:
                {
                    float xoff = RandomFloat(-3, 3), yoff = 0, zoff = RandomFloat(-3, 3);
                    RotateCylin(&xoff, &yoff, &zoff, angle, 0);
                    float t[3];
                    GetTargetCoord(t);
                    x = t[0] + xoff; y = t[1] + yoff + 10; z = t[2] + zoff;
                    float bazimuth = RandomFloat(-M_PI, M_PI);
                    float baltitude = RandomFloat(-M_PI / 2, -M_PI / 4);
                    ax = sin(bazimuth); ay = tan(baltitude); az = cos(bazimuth);
                    if (bazimuth > 0 && bazimuth < M_PI / 12)
                        BulletSpawn(10436101, x, y - 9, z, 0, 0, 0);
                    BulletSpawn(10439003, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside, 0, 0, 0, true);
                    break;
                }
                case WEAPONBUFF1:
                {
                    static bool flag = true;
                    flag ? ApplyEffect(1449000) : ApplyEffect(1446000);
                    flag = !flag;
                    break;
                }
                case WEAPONBUFF2:
                {
                    static bool flag = true;
                    flag ? ApplyEffect(1626000) : ApplyEffect(3160);
                    flag = !flag;
                    break;
                }
                case NOBULLET:
                {
                    BulletSpawn(3251910, x, y, z, ax, ay, az, 1, OwnerID, MagicID, serverside);
                    break;
                }
                case DOANIM:
                {
                    DoAnim(i.bid);
                    break;
                }
                case CHRTURNTO:
                {
                    ChrTurnTo(i.x, i.z);
                    break;
                }
                case SETANIMSPEED: {
                    SetAnimSpeed(i.x);
                    break;
                }
                default:
                case BNORMAL:
                    x = i.x; y = i.y; z = i.z; ax = i.ax; ay = i.ay; az = i.az; NodeID = i.node;
                    BulletSpawn(i.bid, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);
                    break;
                }
                //if (isBullet)
                    //BulletSpawn(i.bid, x, y, z, ax, ay, az, NodeID, OwnerID, MagicID, serverside);
                i.starttime = GetTickCount64() + i.interval;
                i.quantity += i.QuantityChange;
                ++bcountonce;
                if (bcountonce > 256)
                    return;
            }
        }
    }
}

void BulletProcAdd(int key){
    //for (const bullet_t& i : BulletQueue)
        //if (i.key == key && i.quantity != 0)
            //return;
    switch (key) {
    case BSELF: {
        static bool state = true;
        if (state)
        {
            AddBullet(GetPrivateProfileIntW(L"Turret", L"Bullet", 300, INIPath), 654321, GetPrivateProfileIntW(L"Turret", L"Interval", 100, INIPath), BSELF, 378535936, GetTickCount64(), -1, 9, 0, 0, 0, 0, 0, 0, 0, GetPrivateProfileIntW(L"Turret", L"NodeID", 1, INIPath));
        }
        else
            RemoveBullet(BSELF);
        state = !state;
    }
        break;
    case BLIGHTNINGSTORM:
        AddBullet(15, 300, 33, BLIGHTNINGSTORM, 378535936, GetTickCount64(), -1, 9, 1, 15, 0.05);
        break;
    case LIGHTNINGTELEPORT:
        AddBullet(0, 1, 0, LIGHTNINGTELEPORT);
        break;
    case BLIGHTNINGSTORM_SELF:
        AddBullet(15, 300, 33, BLIGHTNINGSTORM_SELF, 378535936, GetTickCount64(), -1, 9, 1, 15, 0.05);
        break;
    case SHADOWTELEPORT:
        float t[3];
        if (GetTargetCoord(t)) {
            DoAnim(60472);
            AddBullet(10653000, 1, 0, BNORMAL, 378535936, GetTickCount64(), -1, 9, 0, 0, 0, 0, 0, 0, 5);
            AddBullet(300, 1, 1, SHADOWTELEPORT, 378535936, GetTickCount64() + 2000);
        }
        break;
    case BGRAVITY: {
        auto BulletParamTable = GetParamTable(10);
        auto addrpull = ParamIDtoAddr(BulletParamTable, 2192);
        *(float*)(addrpull + 0x44) = 25;
        auto addrblackhole = ParamIDtoAddr(BulletParamTable, 10465001);
        *(int*)(addrblackhole + 0xAC) = 2192;
        *(float*)(addrblackhole + 0xB4) = 0.25;
        auto addrpullatk = ParamIDtoAddr(GetParamTable(8), 30300852);
        *(short*)(addrpullatk + 0x52) = 0;
        auto addrexplosion = ParamIDtoAddr(BulletParamTable, 2200372);
        *(int*)(addrexplosion + 0x0) = 40806;
        auto addrburst = ParamIDtoAddr(BulletParamTable, 10732000);
        *(int*)(addrburst + 0x60) = 0;
        AddBullet(10465001, 1, 0, BGRAVITY);
        break;
    }
    case DARKREALM: {
        auto baddr = ParamIDtoAddr(GetParamTable(10), 10503010);
        *(int*)(baddr + 0x0) = 75301;
        baddr = ParamIDtoAddr(GetParamTable(10), 10503011);
        *(int*)(baddr + 0x0) = 75301;
        baddr = ParamIDtoAddr(GetParamTable(15), 4212);
        *(float*)(baddr + 0x8) = 10;
        baddr = ParamIDtoAddr(GetParamTable(15), 4213);
        *(float*)(baddr + 0x8) = 10;
        ApplyEffect(4212);
        ApplyEffect(4213);
        AddBullet(0, 500, 22, DARKREALM, 378535936, GetTickCount64());
        break;
    }
    case FROSTREALM: {
        AddBullet(0, 500, 22, FROSTREALM, 378535936, GetTickCount64());
        break;
    }
    case LIGHTNINGFIST: {
        auto addrburst = ParamIDtoAddr(GetParamTable(10), 10732000);
        *(int*)(addrburst + 0x60) = 0;
        AddBullet(0, 1, 0, SETANIMSPEED, 0, GetTickCount64() + 1100, -1, 9, 0.f, 0, 0, 0, 0, 0, 0, false);
        AddBullet(0, 1, 0, SETANIMSPEED, 0, GetTickCount64() + 3500, -1, 9, 1.f, 0, 0, 0, 0, 0, 0, false);
        AddBullet(10732000, 100, 5, LIGHTNINGFIST, 378535936, GetTickCount64() + 800);
        break;
    }
    case BEAM: {
        AddBullet(0, 100, 100, BEAM, 378535936, GetTickCount64(), 0);
        break;
    }
    case SWORDSUMMON:
    {
        AddBullet(0, 200, 100, SWORDSUMMON, 378535936, GetTickCount64(), -1);
        break;
    }
    case AQUARIUM:
    {
        AddBullet(0, 50, 33, AQUARIUM, 378535936, GetTickCount64() + 1200);
        break;
    }
    case POOPSTORM:
    {
        AddBullet(150, 500, 30, POOPSTORM, 378535936);
        break;
    }
    case SWORDSUMMON_SPHERE:
    {
        AddBullet(150, 1, 0, SWORDSUMMON_SPHERE, 378535936);
        break;
    }
    case SWORDARRAY:
    {
        AddBullet(0, 1, 0, SWORDARRAY);
        break;
    }
    case NOBULLET:
    {
        PBYTE baddr = ParamIDtoAddr(GetParamTable(10), 3251911);
        *(int*)(baddr + 0x0) = 75301;
        baddr = ParamIDtoAddr(GetParamTable(10), 3251912);
        *(int*)(baddr + 0x0) = 75301;
        static bool flag = true;
        flag ? AddBullet(0, 1, 300, NOBULLET, 0, GetTickCount64(), 0) : RemoveBullet(NOBULLET);
        flag = !flag;
        break;
    }
    case WEAPONBUFF1: {
        static bool flag = true;
        flag ? AddBullet(0, 1, 25, WEAPONBUFF1, 0, GetTickCount64(), 0) : RemoveBullet(WEAPONBUFF1);
        flag = !flag;
        break;
    }
    case WEAPONBUFF2: {
        static bool flag = true;
        flag ? AddBullet(0, 1, 25, WEAPONBUFF2, 0, GetTickCount64(), 0) : RemoveBullet(WEAPONBUFF2);
        flag = !flag;
        break;
    }
    case ESP:
    {
        if (!isEspOn) {
            wchar_t diststr[32];
            GetPrivateProfileStringW(L"ESP", L"Distance", L"50.0", diststr, _countof(diststr), INIPath);
            espDist = _wtof(diststr);
        }
        isEspOn = !isEspOn;
        break;
    }
    }
}

void BulletProcRemove(int key) {
    switch (key) {
    case BSELF:
        RemoveBullet(BSELF);
        break;
    case BLIGHTNINGSTORM:
        RemoveBullet(BLIGHTNINGSTORM);
        break;
    case BLIGHTNINGSTORM_SELF:
        RemoveBullet(BLIGHTNINGSTORM_SELF);
        break;
    case BGRAVITY:
        RemoveBullet(BGRAVITY);
        auto Bgravityaddr = ParamIDtoAddr(GetParamTable(10), 2192);
        *(float*)(Bgravityaddr + 0x44) = 7; //Original Value
        auto addrblackhole = ParamIDtoAddr(GetParamTable(10), 10465001);
        *(int*)(addrblackhole + 0xAC) = -1;
        *(float*)(addrblackhole + 0xB4) = 0.05;
        auto addrpullatk = ParamIDtoAddr(GetParamTable(8), 30300852);
        *(short*)(addrpullatk + 0x52) = 60;
        break;
    }
}

void BulletProcLoop() { //Anim, magic etc.
    if (auto panim = AccessMultilevelPointer<int>(WorldChrMan, 0x18468, 0x190, 0x18, 0x40)) {
        static bool ready = true;
        if (*panim == 0 || *panim == 22100 || *panim == 20100 || * panim == 20200 || *panim == 22200 || *panim == 12000000 || *panim == 12020100 || *panim == 2000000 || *panim == 2000100)
            ready = true;
        if (!ready)
            return;
        switch (*panim) {
        case 505045010:
            BulletProcAdd(DARKREALM);
            ready = false;
            break;
        case 42030500:
            BulletProcAdd(LIGHTNINGFIST);
            ready = false;
            break;
        case 407045111:
            BulletProcAdd(BEAM);
            ready = false;
            break;
        case 407045112:
            BulletProcAdd(BEAM);
            ready = false;
            break;
        case 804040000:
            BulletProcAdd(AQUARIUM);
            ready = false;
            break;
        }
    }
}

EXTERN_C char BulletCreationWrapper(char* rcx, char* rdx, char* r8) { // Modify magic
    int& bid = *(int*)(rdx + 0x14);
    int& magicid = *(int*)(rdx + 0xC);
    int& shooter = *(int*)rdx;
    int& nodeid = *(int*)(rdx + 0x1C);
    float& ax = *(float*)(rdx + 0x70);
    float& ay = *(float*)(rdx + 0x74);
    float& az = *(float*)(rdx + 0x78);
    float& x = *(float*)(rdx + 0x80);
    float& y = *(float*)(rdx + 0x84);
    float& z = *(float*)(rdx + 0x88);

    switch (magicid) {
    case 4000: //Glintstone Pebble 
    {
        if (bid == 10400000 && shooter == 0x16900000) {
            for (int i = 0; i < 10; ++i) {
                float azi = atan2(az, ax) + RandomFloat(-M_PI / 18, M_PI / 18); 
                float al = atan2(ay, sqrt(ax * ax + az * az)) + RandomFloat(-M_PI / 18, M_PI / 18);
                AddBullet(bid, 1, 0, BNORMAL, shooter, GetTickCount64(), -1, 9, x, y, z, cos(azi), tan(al), sin(azi), 0);
            }
        }
        break;
    }
    case 4510: //Crystal Release, above target
    {
        float t[3];
        if (GetTargetCoord(t)) {
            x = t[0]; y = t[1] + 2; z = t[2];
            nodeid = 0;
        }
        break;
    }
    case 4210: //Founding Rain of stars, above target
    {
        float t[3];
        if (bid != 10421000	&& GetTargetCoord(t)) {
            x = t[0]; y = t[1]; z = t[2];
            nodeid = 0;
        }
        break;
    }
    case 7530: //Black Blade
    {
        if (bid == 10753020) {
            float azi = atan2(az, ax); float al = atan2(ay, sqrt(ax * ax + az * az));
            for (int v = 0; v < 1; ++v) {
                for (int i = 0; i < 5; ++i) {
                    float random = M_PI / RandomInt(6, 12);
                    float newazi = azi + random;
                    float newazi2 = azi - random;
                    AddBullet(bid, 1, 0, BNORMAL, shooter, GetTickCount64() + 300 * i, -1, 9, x, y, z, cos(newazi), tan(al), sin(newazi), 0);
                    AddBullet(bid, 1, 0, BNORMAL, shooter, GetTickCount64() + 300 * i, -1, 9, x, y, z, cos(newazi2), tan(al), sin(newazi2), 0);
                }
            }
        }
        break;
    }
    case 6940: //Ancient Dragons' Lightning Spear
    {
        if (bid == 10694000) {
            AddBullet(4520325, 1, 0, BNORMAL, shooter, GetTickCount64(), -1, 9, x, y, z, ax, ay, az, nodeid);
        }
        break;
    }
    case 6530: //Darkness
    {
        if (bid == 10653000) {
            for (int i = 0; i < 30; ++i) {
                float r = RandomFloat(2, 10);
                float angle = RandomFloat(0, 2 * M_PI);
                AddBullet(10653000, 1, 0, BNORMAL, shooter, GetTickCount64(), -1, 9, x + r * cos(angle), y, z + r * sin(angle), ax, ay, az, 0);
            }
        }
        break;
    }
    case 6910: // Ancient Dragons' Lightning Strike - Charged
    {
        static const int blist[] = { 4520342,4520343,4520346,4520347 };
        if (bid == 10691070) {
            for (int i = 0; i < 5; ++i) {
                float r = RandomFloat(5, 10);
                float angle = RandomFloat(0, 2 * M_PI);
                AddBullet(blist[RandomInt(0, _countof(blist) - 1)], 5, 300, BNORMAL, shooter, GetTickCount64() + i * 100, -1, 9, x + r * cos(angle), y, z + r * sin(angle), ax, ay, az, 0);
            }
            }
        break;
    }
    case 4830: //Rykard's Rancor
    {
        for (int i = 0; i < 17; ++i) {
            float azi = RandomFloat(-M_PI, M_PI);
            float at = RandomFloat(0, M_PI / 2);
            AddBullet(10483000, 1, 0, BNORMAL, shooter, GetTickCount64(), -1, 9, x, y, z, cos(azi), tan(at), sin(azi), 0);

        }
        break;
    }
    }

    return ((char(*)(char*, char*, char*))BulletCreationFunc)(rcx, rdx, r8);
}
//Bullet End

LRESULT CALLBACK WindowProcHook(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_KEYDOWN:
        if (wParam == 'T' && GetKeyState(VK_CONTROL) < 0) {
            BulletProcAdd(BSELF);
        }
        if (wParam == '1' && GetKeyState(VK_CONTROL) < 0) {
            BulletProcAdd(BLIGHTNINGSTORM);
        }
        if (wParam == '2' && GetKeyState(VK_CONTROL) < 0) {
            BulletProcAdd(BLIGHTNINGSTORM_SELF);
        }
        if (wParam == '3' && GetKeyState(VK_CONTROL) < 0) {
            BulletProcAdd(LIGHTNINGTELEPORT);
        }
        if (wParam == '4' && GetKeyState(VK_CONTROL) < 0) {
            BulletProcAdd(SHADOWTELEPORT);
        }
        if (wParam == '5' && GetKeyState(VK_CONTROL) < 0) {
            BulletProcAdd(BGRAVITY);
        }
        if (wParam == '6' && GetKeyState(VK_CONTROL) < 0) {
            BulletProcAdd(DARKREALM);
        }
        if (wParam == '7' && GetKeyState(VK_CONTROL) < 0) {
            BulletProcAdd(FROSTREALM);
        }
        if (wParam == '8' && GetKeyState(VK_CONTROL) < 0) {
            BulletProcAdd(SWORDSUMMON);
        }
        if (wParam == '9' && GetKeyState(VK_CONTROL) < 0) {
            BulletProcAdd(ESP);
        }
        if (wParam == '0' && GetKeyState(VK_CONTROL) < 0) {
            BulletProcAdd(WEAPONBUFF1);
        }        
        if (wParam == '1' && GetKeyState(VK_SHIFT) < 0) {
            BulletProcAdd(POOPSTORM);
        }
        if (wParam == '2' && GetKeyState(VK_SHIFT) < 0) {
            BulletProcAdd(SWORDSUMMON_SPHERE);
        }
        if (wParam == '3' && GetKeyState(VK_SHIFT) < 0) {
            BulletProcAdd(SWORDARRAY);
        }
        if (wParam == '4' && GetKeyState(VK_SHIFT) < 0) {
            BulletProcAdd(WEAPONBUFF2);
        }
        if (wParam == '5' && GetKeyState(VK_SHIFT) < 0) {
            BulletProcAdd(NOBULLET);
        }

        //if (wParam == 'B' && GetKeyState(VK_CONTROL) < 0) {
            //float t[3];
            //if (GetTargetCoord(t))
                //AddBullet(10501000, 1, 0, BNORMAL, 378535936, GetTickCount64(), -1, 9, t[0], t[1], t[2], 0, 0, 0, 0);
            //BulletProcAdd(DARKREALM);
            //ApplyEffect(4212);
            //BulletProcAdd(BGRAVITY);
            //BulletProcAdd(BEAM);
            //BulletProcAdd(ESP);
            //BulletProcAdd(FROSTREALM);
            //BulletProcAdd(WEAPONBUFF1);
            //BulletProcAdd(SWORDSUMMON);
       //}
        if (wParam == 'M' && GetKeyState(VK_CONTROL) < 0) {
            //BulletProcAdd(SHADOWTELEPORT);
            RemoveBullet(WEAPONBUFF1);
        }
    }
    return ((WNDPROC)WindowProc)(hwnd, uMsg, wParam, lParam);
}

PBYTE AOBScanModule(unsigned char* ImageBase, HMODULE ExeModule, const char* PatternString, size_t SizePattern) { //sizeof PatternString = SizePattern
    size_t PatternNum = (SizePattern + 1) / 3;
    unsigned char* Pattern = (unsigned char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, PatternNum);
    char* bitset = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, PatternNum);
    if (!Pattern || !bitset) {
        return 0;
    }
    const char* p = PatternString;
    for (int i = 0; p < PatternString + SizePattern && i < PatternNum; ++i) {
        auto plast = p;
        Pattern[i] = strtoul(p, (char**)&p, 16);
        if (plast == p) {
            bitset[i] = 1;
            p += 3;
        }
    }

    PBYTE aobptr = 0;
    auto size = PIMAGE_NT_HEADERS(ImageBase + PIMAGE_DOS_HEADER(ImageBase)->e_lfanew)->OptionalHeader.SizeOfImage;
    for (auto m = ImageBase; m + PatternNum < ImageBase + size; ++m) {
        bool found = true;
        for (size_t i = 0; found && i < PatternNum; ++i) {
            if (m[i] != Pattern[i] && !bitset[i]) {
                found = false;
            }
        }
        if (found) {
            aobptr = (PBYTE)ExeModule + (m - ImageBase);
            break;
        }
    }
    HeapFree(GetProcessHeap(), 0, Pattern);
    HeapFree(GetProcessHeap(), 0, bitset);
    return aobptr;
}

EXTERN_C void MainProc() {
    static bool once = true;
    if (once) {
        once = false;
        auto oldproc=SetWindowLongPtrW(GameHwnd, GWLP_WNDPROC, (INT_PTR)WindowProcHook);
        INT_PTR base=(INT_PTR)GetModuleHandleW(0);
        if(base<oldproc&&oldproc<base+PIMAGE_NT_HEADERS(base + PIMAGE_DOS_HEADER(base)->e_lfanew)->OptionalHeader.SizeOfImage)
			WindowProc=(PBYTE)oldproc;
        BulletBase = BulletMan_accessor + *(int*)(BulletMan_accessor + 0x3) + 0x7;
        BulletFunc = BulletMan_accessor + *(int*)(BulletMan_accessor + 0x13) + 0x17;
        //add dmg for custom magic
        PBYTE btable = GetParamTable(10);
        static const int bulletid[] = { 4520325,4520341,4520342,4520343,4520346,4520347 };
        for (int bid : bulletid) {
            PBYTE baddr = ParamIDtoAddr(btable, bid);
            *(int*)(baddr + 0x0) = 75301;
        }
        //end

    }
    BulletProc();
    BulletProcLoop();
    return;
}


EXTERN_C void start()
{
    SetProcessDPIAware();
    Seed = GetTickCount64();
	auto pid = GetGameProcessID();
	HANDLE pHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, pid);
	static const wchar_t NameToFind[] = L"\\eldenring.exe";
	HMODULE ExeModule = GetModuleAddress(pHandle, NameToFind, _countof(NameToFind) - 1);
	//msgbox("%p", ExeModule);
	if (!ExeModule)
		ExitProcess(0);
	wchar_t ExeName[MAX_PATH]; // AOB
	DWORD sz = _countof(ExeName);
	QueryFullProcessImageNameW(pHandle, 0, ExeName, &sz);
	HMODULE hModule = LoadLibraryExW(ExeName, 0, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
	unsigned char* ImageBase = (unsigned char*)((uintptr_t)hModule & ~3); //ImageBase
	static const char aob1[] = "C7 40 1C 00 00 00 00 C7";
	auto InjectPoint = AOBScanModule(ImageBase, ExeModule, aob1, sizeof aob1);
	if (!InjectPoint) {
		MessageBoxW(0, L"Can't Find AOB InjectPoint", 0, 0);
		ExitProcess(0);
	}
	static const char bulletaob[] = "48 8B 0D ?? ?? ?? ?? 4D 8B CE 48 8D 54 24 68 4C 8B C6 E8";
	BulletMan_accessor = AOBScanModule(ImageBase, ExeModule, bulletaob, sizeof bulletaob);
	if (!BulletMan_accessor) {
		MessageBoxW(0, L"Can't Find AOB BulletMan", 0, 0);
		ExitProcess(0);
	}
	static const char windowprocaob[] = "48 89 5C 24 08 4C 89 4C 24 20 4C 89 44 24 18 89 54 24 10 55 48 8B EC 48 83 EC 30 48 8B D9 81 FA";
	WindowProc = AOBScanModule(ImageBase, ExeModule, windowprocaob, sizeof windowprocaob);
	if (!WindowProc) {
		MessageBoxW(0, L"Can't Find AOB windowprocaob", 0, 0);
		ExitProcess(0);
	}
	static const char worldchraob[] = "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 0F 48 39 88 ?? ?? ?? ?? 75 06 89 B1 5C 03 00 00 0F 28 05 ?? ?? ?? ?? 4C 8D 45 E7";
	WorldChrMan = AOBScanModule(ImageBase, 0, worldchraob, sizeof worldchraob);
	if (!WorldChrMan) {
		MessageBoxW(0, L"Can't Find AOB worldchraob", 0, 0);
		ExitProcess(0);
	}
	WorldChrMan = (PBYTE)ExeModule + (INT_PTR)WorldChrMan + *(int*)(ImageBase + (INT_PTR)WorldChrMan + 3) + 7;
	static const char CamHookAOB[] = "0F 29 BB 90 00 00 00";
	auto CamHookPoint = AOBScanModule(ImageBase, ExeModule, CamHookAOB, sizeof CamHookAOB);
	if (!CamHookPoint) {
		MessageBoxW(0, L"Can't Find AOB CamHookAOB", 0, 0);
		ExitProcess(0);
	}
	static const char LockOnHookAOB[] = "48 8B 5C 24 38 33 D2 44 8B";
	auto LockOnHookPoint = AOBScanModule(ImageBase, ExeModule, LockOnHookAOB, sizeof LockOnHookAOB);
	if (!LockOnHookPoint) {
		MessageBoxW(0, L"Can't Find AOB LockOnHookAOB", 0, 0);
		ExitProcess(0);
	}
	static const char CamBaseAOB[] = "48 8B D9 C7 44 24 24 00 00 00 00 48 8B 0D";
	CamBase = AOBScanModule(ImageBase, 0, CamBaseAOB, sizeof CamBaseAOB);
	if (!CamBase) {
		MessageBoxW(0, L"Can't Find AOB CamBaseAOB", 0, 0);
		ExitProcess(0);
	}
	CamBase = (PBYTE)ExeModule + (INT_PTR)CamBase + 18 + *(int*)(ImageBase + (INT_PTR)CamBase + 14);
	static const char ParamBaseAOB[] = "48 8B 0D ?? ?? ?? ?? 45 33 C0 41 8D 50 10 E8 ?? ?? ?? ?? 48 85 C0 0F 84 ?? ?? ?? ?? 48 8B 80";
	ParamBase = AOBScanModule(ImageBase, 0, ParamBaseAOB, sizeof ParamBaseAOB);
	if (!ParamBase) {
		MessageBoxW(0, L"Can't Find AOB ParamBaseAOB", 0, 0);
		ExitProcess(0);
	}
	ParamBase = (PBYTE)ExeModule + (INT_PTR)ParamBase + 7 + *(int*)(ImageBase + (INT_PTR)ParamBase + 3);
	static const char EffectFuncAOB[] = "48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 60 0F B6 84 24 B0 00 00 00 49 8B F1 88 44 24 20 4D 8B F0 8B EA 4C 8B F9 E8 ?? ?? ?? ?? 84 C0 0F 84 ?? ?? ?? ?? 48 83 CF FF 48 89 9C 24 80 00 00 00 48 8B DF";
	EffectFunc = AOBScanModule(ImageBase, ExeModule, EffectFuncAOB, sizeof EffectFuncAOB);
	if (!EffectFunc) {
		MessageBoxW(0, L"Can't Find AOB EffectFuncAOB", 0, 0);
		ExitProcess(0);
	}
	static const char WorldChrBaseAOB[] = "48 8B 41 10 48 85 C0 74 0B 48 8B";
	auto WorldChrBase = AOBScanModule(ImageBase, ExeModule, WorldChrBaseAOB, sizeof WorldChrBaseAOB);
	if (!WorldChrBase) {
		MessageBoxW(0, L"Can't Find AOB WorldChrBaseAOB", 0, 0);
		ExitProcess(0);
	}

	static const char BulletCreationAOB[] = "4C 8B C3 48 8D 54 24 40 48 8B CE E8 ?? ?? ?? ?? 0F B6 D8";
	auto BulletCreationOffset = AOBScanModule(ImageBase, 0, BulletCreationAOB, sizeof BulletCreationAOB);
	if (!WorldChrBase) {
		MessageBoxW(0, L"Can't Find AOB BulletCreationAOB", 0, 0);
		ExitProcess(0);
	}
	BulletCreationInject = (intptr_t)ExeModule + BulletCreationOffset;
	BulletCreationFunc = BulletCreationInject + 16 + *(int*)((intptr_t)ImageBase + BulletCreationOffset + 12);

	unsigned pathlen=GetCurrentDirectoryW(_countof(INIPath),INIPath);
	if(pathlen&&INIPath[pathlen-1]!=L'\\')INIPath[pathlen++]=L'\\';
	wcscpy(INIPath+pathlen,L"config.ini");

	PBYTE selfModule = (PBYTE)GetModuleHandleW(0);//map
	auto ntheader=PIMAGE_NT_HEADERS(selfModule + PIMAGE_DOS_HEADER(selfModule)->e_lfanew);
	DWORD selfSize = ntheader->OptionalHeader.SizeOfImage;
	auto MapHandle = CreateFileMappingW(INVALID_HANDLE_VALUE, 0, PAGE_EXECUTE_READWRITE | SEC_COMMIT, 0, selfSize, 0);
	char* MapSelfBase = (char*)MapViewOfFile2(MapHandle, (HANDLE)-1, 0, 0, 0, 0, PAGE_EXECUTE_READWRITE);
	memcpy(MapSelfBase+ntheader->OptionalHeader.SizeOfHeaders, selfModule+ntheader->OptionalHeader.SizeOfHeaders, selfSize-ntheader->OptionalHeader.SizeOfHeaders);
            
            
            
    HRSRC hRes = FindResourceW(0, (PWSTR)1, (PWSTR)345);
    if (!hRes)
        ExitProcess(0);
    DWORD nRes = SizeofResource(0, hRes);
    HGLOBAL hMem = LoadResource(0, hRes);
    if (!(nRes && hMem))
        ExitProcess(0);
    void* pRes = LockResource(hMem);
    if (!pRes)
        ExitProcess(0);
    wchar_t temppath[MAX_PATH];
    unsigned templen = GetTempPathW(_countof(temppath), temppath);
    __imp_swprintf_s(temppath + templen, _countof(temppath) - templen, L"%u", RtlRandomEx(&Seed));
    HANDLE hfile = CreateFileW(temppath, FILE_WRITE_DATA | SYNCHRONIZE, 7, 0, CREATE_ALWAYS, 0, 0);
    if (hfile == INVALID_HANDLE_VALUE)
        ExitProcess(0);
    DWORD junk;
    IsBadReadPtr(pRes, nRes);
    WriteFile(hfile, pRes, nRes, &junk, 0);
    if (junk != nRes)
        ExitProcess(0);
    CloseHandle(hfile);
    SC_HANDLE hSCM = OpenSCManagerW(0, 0, SC_MANAGER_CREATE_SERVICE | SC_MANAGER_CONNECT);
    SC_HANDLE hService = CreateServiceW(hSCM, L"EACBypassHexER", 0, SERVICE_START | SERVICE_STOP | DELETE, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, 0, temppath, 0, 0, 0, 0, 0);
    
    hfile = CreateFileW(L"\\\\.\\" DEVICE_NAME, FILE_READ_DATA | FILE_WRITE_DATA | SYNCHRONIZE, 7, 0, OPEN_EXISTING, 0, 0);
    if(hfile!=INVALID_HANDLE_VALUE)goto opened;
    if (!hService) {
        hService = OpenServiceW(hSCM, L"EACBypassHexER", SERVICE_START | SERVICE_STOP | SERVICE_CHANGE_CONFIG | DELETE);
        if (hService)ChangeServiceConfigW(hService, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, 0, temppath, 0, 0, 0, 0, 0, 0);
    }
    if (!hService)
        ExitProcess(0);
    if (StartServiceW(hService, 0, 0)||GetLastError()==ERROR_SERVICE_ALREADY_RUNNING) {
        //MessageBoxW(0, L"Drv Started", 0, 0);
        hfile = CreateFileW(L"\\\\.\\" DEVICE_NAME, FILE_READ_DATA | FILE_WRITE_DATA | SYNCHRONIZE, 7, 0, OPEN_EXISTING, 0, 0);
        if (hfile != INVALID_HANDLE_VALUE) {
opened:		hDevice = hfile;
            char* MapTargetBase = (char*)MapSection(hfile, pid, MapHandle, PAGE_EXECUTE_READWRITE);
            //msgbox("MapTargetBase: %p", MapTargetBase);
            //CreateRemoteThread(pHandle, 0, 0, (LPTHREAD_START_ROUTINE)((char*)ThreadProc - (UINT_PTR)selfModule + (UINT_PTR)MapTargetBase), 0, 0, 0);
            emitcall(pid, InjectPoint, globalHook - (char*)selfModule + MapTargetBase, DI, 14);
            emitcall(pid, CamHookPoint, CamHook - (char*)selfModule + MapTargetBase, DI, 14);
            emitcall(pid, LockOnHookPoint, LockOnHook - (char*)selfModule + MapTargetBase, 10, 13);
            emitcall(pid, BulletCreationInject, BulletCreationHook - (char*)selfModule + MapTargetBase, AX, 16);
            emitjump(pid, WorldChrBase, (PBYTE)GameProc - (PBYTE)selfModule + MapTargetBase, AX, 13);
            MessageBoxW(0, L"Scripts have been enabled! Use hotkeys to control them!", L"Successful!", 0);
        cleanup:
            CloseHandle(hfile);
        }

        SERVICE_STATUS ss;
        ControlService(hService, SERVICE_CONTROL_STOP, &ss);
        //msgbox("%d", result);
        //msgbox("Error: %d", GetLastError());
    }
    else
        msgbox("Error: %d", GetLastError());
    DeleteService(hService);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    DeleteFileW(temppath);
    ExitProcess(0);

    //drv end


}