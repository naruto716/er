#define DEVICE_TYPE_DASABI 65534
#define DEVICE_NAME L"EACBypassHexER"
#define IOCTL_WRITEMEM CTL_CODE(DEVICE_TYPE_DASABI,0x800,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_MAPSECTION CTL_CODE(DEVICE_TYPE_DASABI,0x801,METHOD_BUFFERED,FILE_ANY_ACCESS)
struct COPYMEM_PARAMS {
    unsigned pid, len;
    void* from, * to;
};

struct MMAP_PARAMS {
    HANDLE section;
    unsigned pid, prot;
    void* addr;
    SIZE_T len;
};