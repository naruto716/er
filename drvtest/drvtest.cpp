#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ntifs.h>
#include "nt.h"

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

void DriverUnload(PDRIVER_OBJECT DriverObject)
{
	dbgprint("DriverUnload");
}


NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT DriverObject,PUNICODE_STRING RegistryPath)
{
	NTSTATUS status;
	DriverObject->DriverUnload=DriverUnload;
	dbgprint("DriverEntry %wZ",RegistryPath);
	return 0;
}