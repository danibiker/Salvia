// kernel.h
#ifndef _KERNEL_DEFINES_H
#define _KERNEL_DEFINES_H

#include "types.h"
//#include "devio.h"
#include "exconfig.h"
//#include "xekey.h"

#ifdef _XBOX
#else
#include <windows.h>
#endif

#define CONSTANT_OBJECT_STRING(s)   { strlen( s ) / sizeof( OCHAR ), (strlen( s ) / sizeof( OCHAR ))+1, s }
#define MAKE_STRING(s)   {(USHORT)(strlen(s)), (USHORT)((strlen(s))+1), s}

#define STATUS_SUCCESS	0
#define NT_EXTRACT_ST(Status)			((((ULONG)(Status)) >> 30)& 0x3)
#define NT_SUCCESS(Status)              (((NTSTATUS)(Status)) >= 0)
#define NT_INFORMATION(Status)          (NT_EXTRACT_ST(Status) == 1)
#define NT_WARNING(Status)              (NT_EXTRACT_ST(Status) == 2)
#define NT_ERROR(Status)                (NT_EXTRACT_ST(Status) == 3)

#define FILE_SYNCHRONOUS_IO_NONALERT	0x20

// Valid values for the Attributes field
#define OBJ_INHERIT             0x00000002L
#define OBJ_PERMANENT           0x00000010L
#define OBJ_EXCLUSIVE           0x00000020L
#define OBJ_CASE_INSENSITIVE    0x00000040L
#define OBJ_OPENIF              0x00000080L
#define OBJ_OPENLINK            0x00000100L
#define OBJ_VALID_ATTRIBUTES    0x000001F2L

// Directory Stuff
#define DIRECTORY_QUERY                 (0x0001)
#define DIRECTORY_TRAVERSE              (0x0002)
#define DIRECTORY_CREATE_OBJECT         (0x0004)
#define DIRECTORY_CREATE_SUBDIRECTORY   (0x0008)

#define DIRECTORY_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED | 0xF)

#define SYMBOLIC_LINK_QUERY (0x0001)

// object type strings
#define OBJ_TYP_SYMBLINK	0x626d7953
#define OBJ_TYP_DIRECTORY	0x65726944
#define OBJ_TYP_DEVICE		0x69766544


typedef long			NTSTATUS;
typedef ULONG			ACCESS_MASK;

typedef struct _STRING {
    USHORT Length;
    USHORT MaximumLength;
    PCHAR Buffer;
} STRING, *PSTRING;

typedef struct _CSTRING {
	USHORT Length;
	USHORT MaximumLength;
	CONST char *Buffer;
} CSTRING, *PCSTRING;

typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef STRING			OBJECT_STRING;
typedef CSTRING			COBJECT_STRING;
typedef PSTRING			POBJECT_STRING;
typedef PCSTRING		PCOBJECT_STRING;
typedef STRING			OEM_STRING;
typedef PSTRING			POEM_STRING;
typedef CHAR			OCHAR;
typedef CHAR*			POCHAR;
typedef PSTR			POSTR;
typedef PCSTR			PCOSTR;
typedef CHAR*			PSZ;
typedef CONST CHAR*		PCSZ;
typedef STRING			ANSI_STRING;
typedef PSTRING			PANSI_STRING;
typedef CSTRING			CANSI_STRING;
typedef PCSTRING		PCANSI_STRING;
#define ANSI_NULL		((CHAR)0)     // winnt
typedef CONST UNICODE_STRING*	PCUNICODE_STRING;
#define UNICODE_NULL			((WCHAR)0) // winnt

#define OTEXT(quote) __OTEXT(quote)

enum FSINFOCLASS{
	FileFsVolumeInformation = 0x1,
	FileFsLabelInformation = 0x2,
	FileFsSizeInformation = 0x3,
	FileFsDeviceInformation = 0x4,
	FileFsAttributeInformation = 0x5,
	FileFsControlInformation = 0x6,
	FileFsFullSizeInformation = 0x7,
	FileFsObjectIdInformation = 0x8,
	FileFsMaximumInformation = 0x9
};

typedef enum _FILE_INFORMATION_CLASS
{
	FileDirectoryInformation = 0x1,
	FileFullDirectoryInformation = 0x2,
	FileBothDirectoryInformation = 0x3,
	FileBasicInformation = 0x4,
	FileStandardInformation = 0x5,
	FileInternalInformation = 0x6,
	FileEaInformation = 0x7,
	FileAccessInformation = 0x8,
	FileNameInformation = 0x9,
	FileRenameInformation = 0xa,
	FileLinkInformation = 0xb,
	FileNamesInformation = 0xc,
	FileDispositionInformation = 0xd,
	FilePositionInformation = 0xe,
	FileFullEaInformation = 0xf,
	FileModeInformation = 0x10,
	FileAlignmentInformation = 0x11,
	FileAllInformation = 0x12,
	FileAllocationInformation = 0x13,
	FileEndOfFileInformation = 0x14,
	FileAlternateNameInformation = 0x15,
	FileStreamInformation = 0x16,
	FileMountPartitionInformation = 0x17,
	FileMountPartitionsInformation = 0x18,
	FilePipeRemoteInformation = 0x19,
	FileSectorInformation = 0x1a,
	FileXctdCompressionInformation = 0x1b,
	FileCompressionInformation = 0x1c,
	FileObjectIdInformation = 0x1d,
	FileCompletionInformation = 0x1e,
	FileMoveClusterInformation = 0x1f,
	FileIoPriorityInformation = 0x20,
	FileReparsePointInformation = 0x21,
	FileNetworkOpenInformation = 0x22,
	FileAttributeTagInformation = 0x23,
	FileTrackingInformation = 0x24,
	FileMaximumInformation = 0x25
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

typedef struct _IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID Pointer;
    } st;
    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef VOID (NTAPI *PIO_APC_ROUTINE) (
    IN PVOID ApcContext,
    IN PIO_STATUS_BLOCK IoStatusBlock,
    IN ULONG Reserved
);

typedef struct _OBJECT_ATTRIBUTES {
    HANDLE RootDirectory;
    POBJECT_STRING ObjectName;
    ULONG Attributes;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

//++
//
// VOID
// InitializeObjectAttributes(
//     OUT POBJECT_ATTRIBUTES p,
//     IN STRING n,
//     IN ULONG a,
//     IN HANDLE r,
//     )
//--
#define InitializeObjectAttributes( p, n, a, r){ \
	(p)->RootDirectory = r;                             \
	(p)->Attributes = a;                                \
	(p)->ObjectName = n;                                \
}

// returned by a call to 'NtQueryInformationFile' with 0x22 = FileNetworkOpenInformation
typedef struct _FILE_NETWORK_OPEN_INFORMATION {
  LARGE_INTEGER  CreationTime;
  LARGE_INTEGER  LastAccessTime;
  LARGE_INTEGER  LastWriteTime;
  LARGE_INTEGER  ChangeTime;
  LARGE_INTEGER  AllocationSize;
  LARGE_INTEGER  EndOfFile;
  ULONG  FileAttributes;
} FILE_NETWORK_OPEN_INFORMATION, *PFILE_NETWORK_OPEN_INFORMATION;

typedef struct _MM_STATISTICS{
	DWORD Length;
	DWORD TotalPhysicalPages;
	DWORD KernelPages;
	DWORD TitleAvailablePages;
	DWORD TitleTotalVirtualMemoryBytes;
	DWORD TitleReservedVirtualMemoryBytes;
	DWORD TitlePhysicalPages;
	DWORD TitlePoolPages;
	DWORD TitleStackPages;
	DWORD TitleImagePages;
	DWORD TitleHeapPages;
	DWORD TitleVirtualPages;
	DWORD TitlePageTablePages;
	DWORD TitleCachePages;
	DWORD SystemAvailablePages;
	DWORD SystemTotalVirtualMemoryBytes;
	DWORD SystemReservedVirtualMemoryBytes;
	DWORD SystemPhysicalPages;
	DWORD SystemPoolPages;
	DWORD SystemStackPages;
	DWORD SystemImagePages;
	DWORD SystemHeapPages;
	DWORD SystemVirtualPages;
	DWORD SystemPageTablePages;
	DWORD SystemCachePages;
	DWORD HighestPhysicalPage;
} MM_STATISTICS, *PMMSTATISTICS; // 104
//C_ASSERT(sizeof(MM_STATISTICS) == 104);

typedef struct _XBOX_HARDWARE_INFO{
	unsigned long Flags;
	unsigned char NumberOfProcessors;
	unsigned char PCIBridgeRevisionID;
	unsigned char Reserved[6];
	unsigned short BldrMagic;
	unsigned short BldrFlags;
} XBOX_HARDWARE_INFO, *PXBOX_HARDWARE_INFO;

typedef struct _EX_TITLE_TERMINATE_REGISTRATION
{
	void* NotificationRoutine; // function pointer
	u32 Priority; // xam uses 0x7C800000 for early and 0x0 for late
	LIST_ENTRY ListEntry; // already defined in winnt.h
} EX_TITLE_TERMINATE_REGISTRATION, *PEX_TITLE_TERMINATE_REGISTRATION;

#ifdef __cplusplus
extern "C" {
#endif

	extern PXBOX_HARDWARE_INFO XboxHardwareInfo;

	extern PDWORD ExConsoleGameRegion;

	UCHAR WINAPI KeGetCurrentProcessType(VOID);

	void DbgPrint(const char* s, ...);

	// UNTESTED
	VOID RtlInitAnsiString(
		IN OUT	PANSI_STRING DestinationString,
		IN		PCSZ  SourceString
		);

	HRESULT WINAPI ObCreateSymbolicLink(
		IN		PSTRING SymbolicLinkName,
		IN		PSTRING DeviceName
		);

	HRESULT WINAPI ObDeleteSymbolicLink(
		IN		PSTRING SymbolicLinkName
		);

	// 2 is hard reboot, 5 is power down
	VOID WINAPI HalReturnToFirmware(
		IN		DWORD dwPowerDownMode
		); 

	// buffers are 0x10 bytes in length, on cmd that recv no response send NULL for recv
	VOID WINAPI HalSendSMCMessage(
		IN		LPVOID pCommandBuffer,
		OUT		LPVOID pRecvBuffer
		);

	// returns true if the priv has been set
	BOOL WINAPI XexCheckExecutablePrivilege(
		IN		DWORD dwPriv
		);

	//ie XexGetModuleHandle("xam.xex", &hand), returns 0 on success
	DWORD WINAPI XexGetModuleHandle(
		IN		PSZ moduleName,
		IN OUT	PHANDLE hand
		); 

	// ie XexGetProcedureAddress(hand ,0x50, &addr) returns 0 on success
	DWORD WINAPI XexGetProcedureAddress(
		IN		HANDLE hand,
		IN		DWORD dwOrdinal,
		IN		PVOID Address
		);

	DWORD WINAPI ExCreateThread(
		IN		PHANDLE pHandle,
		IN		DWORD dwStackSize,
		IN		LPDWORD lpThreadId,
		IN		PVOID apiThreadStartup,
		IN		LPTHREAD_START_ROUTINE lpStartAddress,
		IN		LPVOID lpParameter,
		IN		DWORD dwCreationFlagsMod
		);

	VOID XapiThreadStartup(
				VOID (__cdecl *StartRoutine)(VOID *),
				PVOID StartContext,
				DWORD dwExitCode
		);

	HANDLE WINAPI XapipCreateThread(
		IN		LPSECURITY_ATTRIBUTES lpThreadAttributes,
		IN		DWORD dwStackSize,
		IN		LPTHREAD_START_ROUTINE lpStartAddress,
		IN		LPVOID lpParameter,
		IN		DWORD dwCreationFlags,
		IN		DWORD dwThreadProcessor,
		IN		LPDWORD lpThreadId,
		IN OUT	PHANDLE pHandle
		);

	NTSTATUS WINAPI ExGetXConfigSetting(
		IN		WORD dwCategory,
		IN		WORD dwSetting,
		OUT		PVOID pBuffer,
		IN		WORD cbBuffer,
		OUT		PWORD szSetting
		);

	NTSTATUS WINAPI ExSetXConfigSetting(
		IN		WORD dwCategory,
		IN		WORD dwSetting,
		IN		PVOID pBuffer,
		IN		WORD szSetting
		);

	// returns the size of the given key number for use with get/set
	WORD WINAPI XeKeysGetKeyProperties(
		IN		DWORD KeyId
		);

	NTSTATUS WINAPI XeKeysGetKey(
		IN		WORD KeyId,
		OUT		PVOID KeyBuffer,
		IN OUT	PWORD keyLength
		);

	NTSTATUS WINAPI XeKeysSetKey(
		IN		WORD KeyId,
		IN		PVOID KeyBuffer,
		IN		WORD keyLength
		);

	NTSTATUS WINAPI XeKeysGenerateRandomKey(
		IN		WORD KeyId,
		OUT		PVOID KeyBuffer
		);

	NTSTATUS WINAPI XeKeysGeneratePrivateKey(
		IN		WORD KeyId
		);

	VOID WINAPI XeKeysGetFactoryChallenge(
		IN		PVOID buf
		);

	DWORD WINAPI XeKeysSetFactoryResponse(
		IN		PVOID buf
		);

	DWORD WINAPI XeKeysGetStatus(
		IN OUT	PDWORD sta
		);

	VOID WINAPI HalOpenCloseODDTray(
		IN		DWORD setTray
		);

	DWORD WINAPI MmQueryAddressProtect(
		IN		PVOID Address
		);

	DWORD WINAPI MmGetPhysicalAddress(
		IN		PVOID Address
		);

	DWORD WINAPI MmIsAddressValid(
		IN		PVOID Address
		);

	DWORD WINAPI MmSetAddressProtect(
		IN		PVOID Address,
		IN		DWORD Size,
		IN		DWORD Type
		);// PAGE_READWRITE

	DWORD WINAPI MmQueryStatistics(
		OUT		PMMSTATISTICS pMmStat
		);

	//INCOMPLETE!!
	PVOID WINAPI MmAllocatePhysicalMemory(
		IN		DWORD unk1, // 0
		IN		DWORD unk2, // 1
		IN		DWORD accessFlags //0x20000004 - gives 1 64k phy alloc
		);

	VOID WINAPI ExRegisterTitleTerminateNotification(
		IN OUT	PEX_TITLE_TERMINATE_REGISTRATION pTermStruct,
		IN		BOOL bCreate // true create, false destroy existing
		);

	NTSTATUS WINAPI NtOpenSymbolicLinkObject(
		OUT		PHANDLE LinkHandle,
		IN		POBJECT_ATTRIBUTES ObjectAttributes
		);

	NTSTATUS WINAPI NtQuerySymbolicLinkObject(
		IN		HANDLE LinkHandle,
		IN OUT	PSTRING LinkTarget,
		OUT		PULONG ReturnedLength OPTIONAL
		);

	NTSTATUS WINAPI NtClose(
		IN		HANDLE Handle
		);

	NTSTATUS WINAPI NtCreateFile(
		OUT		PHANDLE FileHandle,
		IN		ACCESS_MASK DesiredAccess,
		IN		POBJECT_ATTRIBUTES ObjectAttributes,
		OUT		PIO_STATUS_BLOCK IoStatusBlock,
		IN		PLARGE_INTEGER AllocationSize OPTIONAL,
		IN		ULONG FileAttributes,
		IN		ULONG ShareAccess,
		IN		ULONG CreateDisposition,
		IN		ULONG CreateOptions
		);

	NTSTATUS WINAPI NtWriteFile(
		IN		HANDLE FileHandle,
		IN		HANDLE Event OPTIONAL,
		IN		PIO_APC_ROUTINE ApcRoutine OPTIONAL,
		IN		PVOID ApcContext OPTIONAL,
		OUT		PIO_STATUS_BLOCK IoStatusBlock,
		IN		PVOID Buffer,
		IN		ULONG Length,
		IN		PLARGE_INTEGER ByteOffset OPTIONAL
		);

	//UNTESTED
	NTSTATUS WINAPI NtOpenFile(
		OUT		PHANDLE FileHandle,
		IN		ACCESS_MASK DesiredAccess,
		IN		POBJECT_ATTRIBUTES ObjectAttributes,
		OUT		PIO_STATUS_BLOCK IoStatusBlock,
		IN		ULONG ShareAccess,
		IN		ULONG OpenOptions
		);

	//UNTESTED
	NTSTATUS WINAPI NtReadFile(
		IN		HANDLE FileHandle,
		IN		HANDLE Event OPTIONAL,
		IN		PIO_APC_ROUTINE ApcRoutine OPTIONAL,
		IN		PVOID ApcContext OPTIONAL,
		OUT		PIO_STATUS_BLOCK IoStatusBlock,
		OUT		PVOID Buffer,
		IN		ULONG Length,
		IN		PLARGE_INTEGER ByteOffset
		);	

	NTSTATUS WINAPI NtSetInformationFile(
		IN		HANDLE FileHandle,
		OUT		PIO_STATUS_BLOCK IoStatusBlock,
		IN		PVOID FileInformation,
		IN		ULONG Length,
		IN		FILE_INFORMATION_CLASS FileInformationClass
		);

	//UNTESTED
	NTSTATUS WINAPI NtQueryInformationFile(
		IN		HANDLE FileHandle,
		OUT		PIO_STATUS_BLOCK IoStatusBlock,
		OUT		PVOID FileInformation,
		IN		ULONG Length,
		IN		FILE_INFORMATION_CLASS FileInformationClass
		); 

	NTSTATUS WINAPI NtDeviceIoControlFile(
		IN		HANDLE FileHandle,
		IN		HANDLE Event OPTIONAL,
		IN		PIO_APC_ROUTINE ApcRoutine OPTIONAL,
		IN		PVOID ApcContext OPTIONAL,
		OUT		PIO_STATUS_BLOCK IoStatusBlock,
		IN		ULONG IoControlCode,
		IN		PVOID InputBuffer OPTIONAL,
		IN		ULONG InputBufferLength,
		OUT		PVOID OutputBuffer OPTIONAL,
		IN		ULONG OutputBufferLength
		);

	NTSTATUS WINAPI NtOpenDirectoryObject( // 222
		OUT		PHANDLE DirectoryHandle,
		IN		POBJECT_ATTRIBUTES ObjectAttributes
		);

	NTSTATUS WINAPI NtQueryDirectoryObject( // 229
		IN		HANDLE DirectoryHandle,
		OUT		PVOID Buffer OPTIONAL,
		IN		ULONG Length,
		IN		BOOLEAN RestartScan,//__in       BOOLEAN  ReturnSingleEntry,
		IN OUT	PULONG Context,
		OUT		PULONG ReturnLength OPTIONAL
		);

	NTSTATUS WINAPI NtSetSystemTime(
		IN		PFILETIME SystemTime,  // LARGE_INTEGER
		OUT		PFILETIME PreviousTime // LARGE_INTEGER
		);
	
	NTSTATUS WINAPI KeQuerySystemTime(
		OUT		PFILETIME CurrentTime // LARGE_INTEGER
		);

	//NTSTATUS IoCreateDevice(
	//	__in      PDRIVER_OBJECT DriverObject,
	//	__in      ULONG DeviceExtensionSize,
	//	__in_opt  PUNICODE_STRING DeviceName,
	//	__in      DWORD DeviceType, // DEVICE_TYPE
	//	__in      ULONG DeviceCharacteristics,
	//	__out     PDEVICE_OBJECT *DeviceObject
	//	);
#ifdef __cplusplus
}
#endif

#endif	//_KERNEL_DEFINES_H

