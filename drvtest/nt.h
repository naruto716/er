#ifdef _NTDDK_
EXTERN_C __declspec(dllimport) POBJECT_TYPE*MmSectionObjectType;
EXTERN_C NTSTATUS NTSYSAPI NTAPI ObOpenObjectByName(POBJECT_ATTRIBUTES ObjectAttributes,POBJECT_TYPE ObjectType,KPROCESSOR_MODE AccessMode,PACCESS_STATE AccessState,ACCESS_MASK DesiredAccess,PVOID ParseContext,PHANDLE Handle);
EXTERN_C NTSTATUS NTSYSAPI NTAPI ObReferenceObjectByName(PUNICODE_STRING ObjectName,ULONG Attributes,PACCESS_STATE AccessState,ACCESS_MASK DesiredAccess,POBJECT_TYPE ObjectType,KPROCESSOR_MODE AccessMode,PVOID ParseContext,PVOID *Object);
#define defoa(name,str) static UNICODE_STRING name##_us={sizeof(str)-sizeof*str,sizeof(str)-sizeof*str,(wchar_t*)str};static OBJECT_ATTRIBUTES name={sizeof(OBJECT_ATTRIBUTES),0,&name##_us,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE};
#else
#ifndef _In_
#define _In_
#define _In_opt_
#define _In_reads_opt_(x)
#define _Out_
#define _Inout_
#define _Out_opt_
#define _When_(...)
#define _Field_size_(x)
#define _Field_size_opt_(x)
#define _Success_(x)
#define _Out_writes_bytes_(x)
#define _In_z_
#define _In_reads_(x)
#define _In_opt_z_
#define _Out_writes_to_opt_(...)
#define _Out_writes_bytes_to_(...)
#define _In_reads_bytes_opt_(...)
#define _Out_writes_bytes_to_opt_(...)
#define _In_reads_bytes_opt_(...)
#define _Outptr_result_maybenull_
#define _Out_range_(...)
#define _In_range_(...)
#define _Outptr_result_buffer_(...)
#define _Outptr_result_bytebuffer_(...)
#define _In_reads_bytes_(...)
#define _Out_writes_opt_(...)
#define _Out_writes_bytes_opt_(...)
#define _Inout_updates_(...)
#define _Strict_type_match_
#define _Return_type_success_(x)
#endif
#include <winternl.h>
EXTERN_C NTSTATUS NTSYSAPI NTAPI RtlAdjustPrivilege(ULONG Privilege,BOOLEAN Enable,BOOLEAN CurrentThread,PBOOLEAN Enabled);
EXTERN_C NTSTATUS NTSYSAPI NTAPI LdrLoadDll(PWSTR SearchPath,PULONG DllCharacteristics,PUNICODE_STRING DllName,PVOID*BaseAddress);
EXTERN_C NTSTATUS NTSYSAPI NTAPI LdrGetProcedureAddress(PVOID BaseAddress,PANSI_STRING Name,ULONG Ordinal,PVOID*ProcedureAddress);
EXTERN_C NTSYSAPI NTSTATUS NTAPI NtTerminateProcess(HANDLE,NTSTATUS);
EXTERN_C NTSYSAPI HWND NTAPI NtUserFindWindowEx(HWND hwndParent,HWND hwndChildAfter,PUNICODE_STRING ClassName,PUNICODE_STRING WindowName,DWORD Unknown);
EXTERN_C NTSYSAPI BOOL NTAPI NtUserPostMessage(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam);
#define defoa(name,str) static UNICODE_STRING name##_us={sizeof(str)-sizeof*str,sizeof(str)-sizeof*str,(wchar_t*)str};static OBJECT_ATTRIBUTES name={sizeof(OBJECT_ATTRIBUTES),0,&name##_us,OBJ_CASE_INSENSITIVE};
#endif
#define defus(name,str) static UNICODE_STRING name={sizeof(str)-sizeof*str,sizeof(str)-sizeof*str,(wchar_t*)str};
#define allocvar(type,name,size) char name##buf##__LINE__[size];type&name=(type&)name##buf##__LINE__;
#define allocptr(type,name,size) char name##buf##__LINE__[size];type name=(type)name##buf##__LINE__;

#define 	SE_CREATE_TOKEN_PRIVILEGE   (2L)
#define 	SE_ASSIGNPRIMARYTOKEN_PRIVILEGE   (3L)
#define 	SE_LOCK_MEMORY_PRIVILEGE   (4L)
#define 	SE_INCREASE_QUOTA_PRIVILEGE   (5L)
#define 	SE_MACHINE_ACCOUNT_PRIVILEGE   (6L)
#define 	SE_TCB_PRIVILEGE   (7L)
#define 	SE_SECURITY_PRIVILEGE   (8L)
#define 	SE_TAKE_OWNERSHIP_PRIVILEGE   (9L)
#define 	SE_LOAD_DRIVER_PRIVILEGE   (10L)
#define 	SE_SYSTEM_PROFILE_PRIVILEGE   (11L)
#define 	SE_SYSTEMTIME_PRIVILEGE   (12L)
#define 	SE_PROF_SINGLE_PROCESS_PRIVILEGE   (13L)
#define 	SE_INC_BASE_PRIORITY_PRIVILEGE   (14L)
#define 	SE_CREATE_PAGEFILE_PRIVILEGE   (15L)
#define 	SE_CREATE_PERMANENT_PRIVILEGE   (16L)
#define 	SE_BACKUP_PRIVILEGE   (17L)
#define 	SE_RESTORE_PRIVILEGE   (18L)
#define 	SE_SHUTDOWN_PRIVILEGE   (19L)
#define 	SE_DEBUG_PRIVILEGE   (20L)
#define 	SE_AUDIT_PRIVILEGE   (21L)
#define 	SE_SYSTEM_ENVIRONMENT_PRIVILEGE   (22L)
#define 	SE_CHANGE_NOTIFY_PRIVILEGE   (23L)
#define 	SE_REMOTE_SHUTDOWN_PRIVILEGE   (24L)
#define 	SE_UNDOCK_PRIVILEGE   (25L)
#define 	SE_SYNC_AGENT_PRIVILEGE   (26L)
#define 	SE_ENABLE_DELEGATION_PRIVILEGE   (27L)
#define 	SE_MANAGE_VOLUME_PRIVILEGE   (28L)
#define 	SE_IMPERSONATE_PRIVILEGE   (29L)
#define 	SE_CREATE_GLOBAL_PRIVILEGE   (30L)
#define 	SE_TRUSTED_CREDMAN_ACCESS_PRIVILEGE   (31L)
#define 	SE_RELABEL_PRIVILEGE   (32L)
#define 	SE_INC_WORKING_SET_PRIVILEGE   (33L)
#define 	SE_TIME_ZONE_PRIVILEGE   (34L)
#define 	SE_CREATE_SYMBOLIC_LINK_PRIVILEGE   (35L)
EXTERN_C int(*__imp_vsprintf_s)(char *buffer,size_t numberOfElements,const char *format,va_list argptr);
EXTERN_C NTSYSAPI NTSTATUS NTAPI NtSetSystemInformation(int SystemInformationClass,PVOID SystemInformation,ULONG SystemInformationLength);