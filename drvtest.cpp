#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ntifs.h>
#include "nt.h"
#include "drv.h"
#define PROCESS_VM_OPERATION               (0x0008) 
EXTERN_C NTSTATUS NTSYSAPI NTAPI MmCopyVirtualMemory(PEPROCESS FromProcess, CONST VOID* FromAddress, PEPROCESS ToProcess, PVOID ToAddress, SIZE_T BufferSize, KPROCESSOR_MODE PreviousMode, PSIZE_T NumberOfBytesCopied);

PDEVICE_OBJECT DeviceObject;

NTSTATUS dbgprint(const char*format,...)
{
	NTSTATUS status;
	defus(DBWIN_BUFFER_name,L"\\BaseNamedObjects\\DBWIN_BUFFER")
	defus(DBWIN_BUFFER_READY_name,L"\\BaseNamedObjects\\DBWIN_BUFFER_READY")
	defus(DBWIN_DATA_READY_name,L"\\BaseNamedObjects\\DBWIN_DATA_READY")
	PVOID DBWinMutex,DBWIN_BUFFER,DBWIN_BUFFER_READY,DBWIN_DATA_READY;
	LARGE_INTEGER Timeout;
	Timeout.QuadPart=-40000000;
	if((status=ObReferenceObjectByName(&DBWIN_BUFFER_READY_name,0,0,SYNCHRONIZE,*ExEventObjectType,KernelMode,0,&DBWIN_BUFFER_READY))==0){
		if((status=KeWaitForSingleObject(DBWIN_BUFFER_READY,Executive,KernelMode,0,&Timeout))==0&&(status=ObReferenceObjectByName(&DBWIN_BUFFER_name,0,0,SECTION_MAP_READ|SECTION_MAP_WRITE,*MmSectionObjectType,KernelMode,0,&DBWIN_BUFFER))==0){
			struct{UINT32 pid;char data[4096-sizeof(UINT32)];}*dbwin_buffer=0;SIZE_T ViewSize=0;
			if((status=MmMapViewInSystemSpace(DBWIN_BUFFER,(void**)&dbwin_buffer,&ViewSize)==0)&&ViewSize>=sizeof*dbwin_buffer){
				dbwin_buffer->pid=(UINT_PTR)PsGetCurrentProcessId();
				va_list args;
				va_start(args,format);
				__imp_vsprintf_s(dbwin_buffer->data,sizeof dbwin_buffer->data,format,args);
				va_end(args);
				MmUnmapViewInSystemSpace(dbwin_buffer);
				if((status=ObReferenceObjectByName(&DBWIN_DATA_READY_name,0,0,EVENT_MODIFY_STATE,*ExEventObjectType,KernelMode,0,&DBWIN_DATA_READY))==0){
					status=KeSetEvent((PRKEVENT)DBWIN_DATA_READY,0,0);
					ObDereferenceObject(DBWIN_DATA_READY);
				}
			}
			ObDereferenceObject(DBWIN_BUFFER);
		}
		ObDereferenceObject(DBWIN_BUFFER_READY);
	}
	return status;
}

static NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject,PIRP Irp)
{
    Irp->IoStatus.Status=Irp->IoStatus.Information=0;
    IofCompleteRequest(Irp,IO_NO_INCREMENT);
    return 0;
}


static NTSTATUS DispatchControl(PDEVICE_OBJECT DeviceObject,PIRP Irp)
{
    NTSTATUS status=0;
    Irp->IoStatus.Information=0;
    PIO_STACK_LOCATION iosl=IoGetCurrentIrpStackLocation(Irp);
    ULONG IoControlCode=iosl->Parameters.DeviceIoControl.IoControlCode;
    switch(IoControlCode){
		case IOCTL_MAPSECTION:
		{
			if (iosl->Parameters.DeviceIoControl.InputBufferLength == sizeof(MMAP_PARAMS) && iosl->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(MMAP_PARAMS)) {
				dbgprint("Drv PID: %d", ((MMAP_PARAMS*)(Irp->AssociatedIrp.SystemBuffer))->pid);
				HANDLE pHandle;
				OBJECT_ATTRIBUTES oa = { .Attributes = OBJ_KERNEL_HANDLE };
				CLIENT_ID cid;
				cid.UniqueProcess = (HANDLE)(UINT_PTR)((MMAP_PARAMS*)(Irp->AssociatedIrp.SystemBuffer))->pid;
				cid.UniqueThread = 0;
				status = ZwOpenProcess(&pHandle, PROCESS_VM_OPERATION, &oa, &cid);
				if (status == 0) {
					status = ZwMapViewOfSection(((MMAP_PARAMS*)(Irp->AssociatedIrp.SystemBuffer))->section, pHandle, &((MMAP_PARAMS*)(Irp->AssociatedIrp.SystemBuffer))->addr, 0, 0, 0, &((MMAP_PARAMS*)(Irp->AssociatedIrp.SystemBuffer))->len, ViewUnmap, 0, ((MMAP_PARAMS*)(Irp->AssociatedIrp.SystemBuffer))->prot);
					if (status == STATUS_SUCCESS) {
						Irp->IoStatus.Information = sizeof(MMAP_PARAMS);

					}
					ZwClose(pHandle);
				}
			}
			else
				status = STATUS_INFO_LENGTH_MISMATCH;
			break;
		}
		case IOCTL_WRITEMEM:
		{
			if (iosl->Parameters.DeviceIoControl.InputBufferLength == sizeof(COPYMEM_PARAMS)) {
				PEPROCESS pp;
				status = PsLookupProcessByProcessId((HANDLE)(UINT_PTR)((COPYMEM_PARAMS*)(Irp->AssociatedIrp.SystemBuffer))->pid, &pp);
				if (status == STATUS_SUCCESS) {
					size_t junk;
					dbgprint("TargetAddr: %p\nLen: %d", ((COPYMEM_PARAMS*)(Irp->AssociatedIrp.SystemBuffer))->to, ((COPYMEM_PARAMS*)(Irp->AssociatedIrp.SystemBuffer))->len);
					status = MmCopyVirtualMemory(PsGetCurrentProcess(), ((COPYMEM_PARAMS*)(Irp->AssociatedIrp.SystemBuffer))->from, pp, ((COPYMEM_PARAMS*)(Irp->AssociatedIrp.SystemBuffer))->to, ((COPYMEM_PARAMS*)(Irp->AssociatedIrp.SystemBuffer))->len, KernelMode, &junk);
					ObDereferenceObject(pp);
				}
			}
			break;
		}
    }
    Irp->IoStatus.Status=status;
    IofCompleteRequest(Irp,IO_NO_INCREMENT);
    return status;
}

void DriverUnload(PDRIVER_OBJECT DriverObject)
{
	dbgprint("DriverUnload");
	IoDeleteDevice(DeviceObject);
}


EXTERN_C NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    defus(DeviceName, L"\\??\\" DEVICE_NAME)
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchControl;
    DriverObject->DriverUnload = DriverUnload;
    NTSTATUS status = IoCreateDevice(DriverObject,0,&DeviceName,DEVICE_TYPE_DASABI,0,0,&DeviceObject);
    dbgprint("DriverEntry %x",status);
    return status;
} 
