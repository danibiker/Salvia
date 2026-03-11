#ifdef _XBOX
#define _WIN32_WINNT 0x0500
#include <xtl.h>

  #ifndef SQLITE_PTRSIZE
    #define SQLITE_PTRSIZE 4
  #endif
  
  // ¡MUY IMPORTANTE! La Xbox 360 es Big-Endian. 
  // Si no defines esto, los números en la DB saldrán al revés.
  #ifndef SQLITE_BYTEORDER
    #define SQLITE_BYTEORDER 4321
  #endif

#ifndef _SQLITE_XBOX_CONSTANTS_
#define _SQLITE_XBOX_CONSTANTS_

// 1. Codificación de caracteres (La Xbox 360 no usa tablas OEM de Windows)
#define CP_OEMCP 1

// 2. Identificación de plataforma (Simulamos NT/XP)
#ifndef VER_PLATFORM_WIN32_NT
#define VER_PLATFORM_WIN32_NT 2
#endif

// 3. Flags para FormatMessage (Aunque usemos un Stub, el código necesita las constantes)
#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#define FORMAT_MESSAGE_IGNORE_INSERTS  0x00000200
#define FORMAT_MESSAGE_FROM_SYSTEM     0x00001000

// 4. Flags para Mapeo de Archivos (No existen en XDK porque no hay MMAP)
#define FILE_MAP_WRITE 0x0002
#define FILE_MAP_READ  0x0004

// 5. Flags de Bloqueo de Archivos (Por si te aparecen luego)
#ifndef LOCKFILE_FAIL_IMMEDIATELY
#define LOCKFILE_FAIL_IMMEDIATELY   0x00000001
#define LOCKFILE_EXCLUSIVE_LOCK     0x00000002
#endif

#endif

// Helper para convertir WCHAR* a CHAR* (necesario para el XDK)
void WideToAnsi(LPCWSTR src, char* dest, int destLen) {
    if (src == NULL) { dest[0] = '\0'; return; }
    // En XDK wcstombs es la forma estándar de conversión
    wcstombs(dest, src, destLen);
}

HANDLE WINAPI CreateFileXBoxW(LPCWSTR lpFileName, DWORD dwAccess, DWORD dwShare, 
                             LPSECURITY_ATTRIBUTES lpSA, DWORD dwCreation, 
                             DWORD dwFlags, HANDLE hTemplate) {
    char fileNameAnsi[MAX_PATH];
	DWORD xboxShare = FILE_SHARE_READ; // Más seguro en XDK
    DWORD xboxFlags = FILE_ATTRIBUTE_NORMAL;
	HANDLE hFile;

    WideToAnsi(lpFileName, fileNameAnsi, MAX_PATH);

    // FORZADO PARA XBOX 360:
    // 1. Quitamos FILE_SHARE_WRITE si vamos a escribir nosotros, para evitar conflictos.
    // 2. Nos aseguramos de que no se use FILE_FLAG_OVERLAPPED (no soportado igual que en PC).
    hFile = CreateFileA(fileNameAnsi, dwAccess, xboxShare, lpSA, dwCreation, xboxFlags, hTemplate);

    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        // Aquí puedes poner un breakpoint para ver si falla por ERROR_ACCESS_DENIED (5)
    }

    return hFile;
}

// 2. Mock de AreFileApisANSI (En Xbox siempre es ANSI)
BOOL WINAPI AreFileApisANSI_XBox() { return TRUE; }

// Adaptador para GetFullPathNameA (Versión ANSI)
DWORD WINAPI GetFullPathNameXBoxA(
    LPCSTR lpFileName,
    DWORD nBufferLength,
    LPSTR lpBuffer,
    LPSTR *lpFilePart
) {
	DWORD len;
    if (lpFileName == NULL) return 0;

    len = (DWORD)strlen(lpFileName);

    // Si nBufferLength es 0, SQLite está pidiendo el tamaño necesario del buffer
    if (nBufferLength == 0) {
        return len + 1; 
    }

    // Si el buffer es suficiente, copiamos la ruta
    if (lpBuffer && nBufferLength > len) {
        strcpy(lpBuffer, lpFileName);
        
        // Buscamos la última barra para identificar el nombre del archivo
        if (lpFilePart) {
            char *lastSlash = strrchr(lpBuffer, '\\');
            *lpFilePart = (lastSlash) ? (lastSlash + 1) : lpBuffer;
        }
        return len;
    }

    // Si el buffer es demasiado pequeño, devolvemos el tamaño necesario
    return len + 1;
}

// Adaptador para GetFullPathNameW (Versión Unicode)
DWORD WINAPI GetFullPathNameXBoxW(
    LPCWSTR lpFileName,
    DWORD nBufferLength,
    LPWSTR lpBuffer,
    LPWSTR *lpFilePart
) {
    DWORD len = (DWORD)wcslen(lpFileName);
    
    // Si nBufferLength es 0, SQLite quiere saber el tamaño necesario
    if (nBufferLength == 0) {
        return len + 1; 
    }

    // Si hay buffer, copiamos la ruta (en Xbox las rutas suelen ser ya absolutas)
    if (lpBuffer && nBufferLength > len) {
        wcscpy(lpBuffer, lpFileName);
        if (lpFilePart) {
            WCHAR *lastSlash = wcsrchr(lpBuffer, L'\\');
            *lpFilePart = (lastSlash) ? (lastSlash + 1) : lpBuffer;
        }
        return len;
    }
    
    return 0; // Error: buffer insuficiente
}

// Adaptador para GetDiskFreeSpaceA
// SQLite usa esto para verificar el espacio en disco.
BOOL WINAPI GetDiskFreeSpaceXBoxA(
    LPCSTR lpRootPathName,
    LPDWORD lpSectorsPerCluster,
    LPDWORD lpBytesPerSector,
    LPDWORD lpNumberOfFreeClusters,
    LPDWORD lpTotalNumberOfClusters
) {
    // Valores estándar compatibles con el sistema de archivos de la 360
    if (lpSectorsPerCluster) *lpSectorsPerCluster = 8;
    if (lpBytesPerSector)    *lpBytesPerSector = 512; // Sector estándar de 512 bytes
    
    // Devolvemos un valor alto fijo para simular que siempre hay espacio.
    // 0x7FFFFFFF es un valor seguro para evitar overflows en cálculos de 32 bits.
    if (lpNumberOfFreeClusters) *lpNumberOfFreeClusters = 0x7FFFFFFF;
    if (lpTotalNumberOfClusters) *lpTotalNumberOfClusters = 0x7FFFFFFF;

    return TRUE; // Éxito
}

// También la versión Unicode por si acaso SQLite la busca
BOOL WINAPI GetDiskFreeSpaceXBoxW(LPCWSTR lpRoot, LPDWORD lpS, LPDWORD lpB, LPDWORD lpF, LPDWORD lpT) {
    return GetDiskFreeSpaceXBoxA(NULL, lpS, lpB, lpF, lpT);
}

// 1. Adaptador para CreateMutexW
// SQLite lo usa para control de concurrencia. La 360 usa CreateMutexA.
HANDLE WINAPI CreateMutexXBoxW(LPSECURITY_ATTRIBUTES lpSA, BOOL bInitialOwner, LPCWSTR lpName) {
    char nameAnsi[MAX_PATH];
	if (lpName == NULL) {
        return CreateMutexA(lpSA, bInitialOwner, NULL);
    }
    WideToAnsi(lpName, nameAnsi, MAX_PATH);
    return CreateMutexA(lpSA, bInitialOwner, nameAnsi);
}

// 2. Adaptador para DeleteFileW
// Sin esto, SQLite no puede borrar archivos temporales o de rollback.
BOOL WINAPI DeleteFileXBoxW(LPCWSTR lpFileName) {
    char pathAnsi[MAX_PATH];
    WideToAnsi(lpFileName, pathAnsi, MAX_PATH);
    return DeleteFileA(pathAnsi);
}

// 1. Definición de la estructura si el compilador no la encuentra (a veces pasa en XDK antiguo)
/*#ifndef _FILE_ATTRIBUTE_DATA_DEFINED
typedef struct _WIN32_FILE_ATTRIBUTE_DATA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
} WIN32_FILE_ATTRIBUTE_DATA, *LPWIN32_FILE_ATTRIBUTE_DATA;
#endif*/

// 2. Adaptador para GetFileAttributesExW
// SQLite usa esto para obtener metadatos del archivo de una sola vez.
BOOL WINAPI GetFileAttributesExXBoxW(
    LPCWSTR lpFileName, 
    GET_FILEEX_INFO_LEVELS fInfoLevelId, 
    LPVOID lpFileInformation
) {
    char pathAnsi[MAX_PATH];
    WideToAnsi(lpFileName, pathAnsi, MAX_PATH);

    // El XDK soporta GetFileAttributesExA (ANSI)
    // Si fInfoLevelId es GetFileExInfoStandard, mapeamos directamente.
    return GetFileAttributesExA(pathAnsi, fInfoLevelId, lpFileInformation);
}

// 3. Adaptador para GetFileAttributesW (Muy usado por SQLite para ver si existe la DB)
DWORD WINAPI GetFileAttributesXBoxW(LPCWSTR lpFileName) {
    char pathAnsi[MAX_PATH];
    WideToAnsi(lpFileName, pathAnsi, MAX_PATH);
    return GetFileAttributesA(pathAnsi);
}

// Adaptador para FormatMessageW (La Xbox 360 no tiene catálogo de errores)
DWORD WINAPI FormatMessageXBoxW(
    DWORD dwFlags,
    LPCVOID lpSource,
    DWORD dwMessageId,
    DWORD dwLanguageId,
    LPWSTR lpBuffer,
    DWORD nSize,
    va_list *Arguments
) {
    // SQLite suele usar FORMAT_MESSAGE_FROM_SYSTEM. 
    // Como no hay mensajes, escribimos un error genérico en el buffer.
    if (lpBuffer && nSize > 20) {
        swprintf(lpBuffer, nSize, L"XBox Error Code: %d", dwMessageId);
        return (DWORD)wcslen(lpBuffer);
    }
    return 0;
}

// La Xbox 360 no tiene múltiples procesos, devolvemos un ID fijo o el ID del hilo
DWORD WINAPI GetCurrentProcessIdXBox() {
    // 0x360 es un valor simpático y único para identificar el "proceso" de la consola
    return 0x360; 
}

// La Xbox 360 no soporta carga dinámica de funciones
FARPROC WINAPI GetProcAddressXBoxA(HMODULE hModule, LPCSTR lpProcName) {
    // Siempre devolvemos NULL porque no hay símbolos dinámicos
    return NULL; 
}

// También necesitarás el stub para GetModuleHandleW si te da error
HMODULE WINAPI GetModuleHandleXBoxW(LPCWSTR lpModuleName) {
    return NULL;
}

// Definición de la estructura para el XDK
typedef struct _SYSTEM_INFO {
    union {
        DWORD dwOemId;
        struct {
            WORD wProcessorArchitecture;
            WORD wReserved;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
    DWORD dwPageSize;
    LPVOID lpMinimumApplicationAddress;
    LPVOID lpMaximumApplicationAddress;
    DWORD_PTR dwActiveProcessorMask;
    DWORD dwNumberOfProcessors;
    DWORD dwProcessorType;
    DWORD dwAllocationGranularity;
    WORD wProcessorLevel;
    WORD wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

// Implementación del adaptador
void WINAPI GetSystemInfoXBox(LPSYSTEM_INFO lpSystemInfo) {
    if (lpSystemInfo) {
        memset(lpSystemInfo, 0, sizeof(SYSTEM_INFO));
        lpSystemInfo->dwPageSize = 4096;               // 4KB estándar
        lpSystemInfo->dwAllocationGranularity = 65536; // 64KB estándar de Windows/Xbox
        lpSystemInfo->dwNumberOfProcessors = 3;        // La 360 tiene 3 núcleos (6 hilos)
    }
}

// También suele pedir GetNativeSystemInfo
void WINAPI GetNativeSystemInfoXBox(LPSYSTEM_INFO lpSystemInfo) {
    GetSystemInfoXBox(lpSystemInfo);
}

// La Xbox 360 no tiene una carpeta "Temp" global. 
// Como usas SQLITE_TEMP_STORE 3, esta función nunca se ejecutará, 
// pero debe existir para que el compilador sea feliz.
DWORD WINAPI GetTempPathXBoxW(DWORD nBufferLength, LPWSTR lpBuffer) {
    if (lpBuffer && nBufferLength > 7) {
        // "Z:\\" suele ser el punto de montaje del DVD o la caché en XDK, 
        // pero da igual porque no se usará.
        wcscpy(lpBuffer, L"cache:\\"); 
        return (DWORD)wcslen(lpBuffer);
    }
    return 0;
}

// También es recomendable añadir la versión ANSI por si acaso
DWORD WINAPI GetTempPathXBoxA(DWORD nBufferLength, LPSTR lpBuffer) {
    if (lpBuffer && nBufferLength > 7) {
        strcpy(lpBuffer, "cache:\\");
        return (DWORD)strlen(lpBuffer);
    }
    return 0;
}

// Estructura Unicode para versión (faltante en XDK)
typedef struct _OSVERSIONINFOW {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    WCHAR szCSDVersion[128];
} OSVERSIONINFOW, *POSVERSIONINFOW, *LPOSVERSIONINFOW;

// Adaptador para GetVersionExW
// Simulamos un kernel compatible con NT (la Xbox 360 se basa en el kernel de Win2000/XP)
BOOL WINAPI GetVersionExXBoxW(LPOSVERSIONINFOW lpOS) {
    if(lpOS) {
        memset(lpOS, 0, sizeof(OSVERSIONINFOW));
        lpOS->dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
        lpOS->dwMajorVersion = 5; 
        lpOS->dwMinorVersion = 0;
        lpOS->dwPlatformId = 2; // VER_PLATFORM_WIN32_NT
        wcscpy(lpOS->szCSDVersion, L"XBox360");
    }
    return TRUE;
}

// Estructura ANSI para versión (faltante en XDK)
typedef struct _OSVERSIONINFOA {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    CHAR  szCSDVersion[128];
} OSVERSIONINFOA, *POSVERSIONINFOA, *LPOSVERSIONINFOA;

// Adaptador para GetVersionExA
// Simulamos un kernel NT 5.0 (Windows 2000), que es el ancestro del kernel de la 360.
BOOL WINAPI GetVersionExXBoxA(LPOSVERSIONINFOA lpOS) {
    if(lpOS) {
        memset(lpOS, 0, sizeof(OSVERSIONINFOA));
        lpOS->dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
        lpOS->dwMajorVersion = 5; 
        lpOS->dwMinorVersion = 0;
        lpOS->dwPlatformId = VER_PLATFORM_WIN32_NT;
        strcpy(lpOS->szCSDVersion, "XBox360");
    }
    return TRUE;
}

// Adaptador para LockFile
// En Xbox 360 no hay concurrencia de procesos, devolvemos siempre éxito.
BOOL WINAPI LockFileXBox(
    HANDLE hFile,
    DWORD dwFileOffsetLow,
    DWORD dwFileOffsetHigh,
    DWORD nNumberOfBytesToLockLow,
    DWORD nNumberOfBytesToLockHigh
) {
    return TRUE; 
}

// Adaptador para UnlockFile
BOOL WINAPI UnlockFileXBox(
    HANDLE hFile,
    DWORD dwFileOffsetLow,
    DWORD dwFileOffsetHigh,
    DWORD nNumberOfBytesToUnlockLow,
    DWORD nNumberOfBytesToUnlockHigh
) {
    return TRUE;
}

// Adaptador para LockFileEx (Versión extendida con flags)
BOOL WINAPI LockFileExXBox(
    HANDLE hFile,
    DWORD dwFlags,
    DWORD dwReserved,
    DWORD nNumberOfBytesToLockLow,
    DWORD nNumberOfBytesToLockHigh,
    LPOVERLAPPED lpOverlapped
) {
    return TRUE;
}

// Adaptador para UnlockFileEx
BOOL WINAPI UnlockFileExXBox(
    HANDLE hFile,
    DWORD dwReserved,
    DWORD nNumberOfBytesToUnlockLow,
    DWORD nNumberOfBytesToUnlockHigh,
    LPOVERLAPPED lpOverlapped
) {
    return TRUE;
}

// La Xbox 360 no tiene soporte para Memory Mapped I/O
LPVOID WINAPI MapViewOfFileXBox(
    HANDLE hFileMappingObject,
    DWORD dwDesiredAccess,
    DWORD dwFileOffsetHigh,
    DWORD dwFileOffsetLow,
    SIZE_T dwNumberOfBytesToMap
) {
    return NULL; // El mapeo siempre falla en 360
}

BOOL WINAPI UnmapViewOfFileXBox(LPCVOID lpBaseAddress) {
    return FALSE; 
}

BOOL WINAPI FlushViewOfFileXBox(LPCVOID lpBaseAddress, SIZE_T dwNumberOfBytesToFlush) {
    return FALSE;
}

// También necesitarás los stubs de las funciones que "crean" el mapeo
HANDLE WINAPI CreateFileMappingXBoxA(HANDLE hFile, LPSECURITY_ATTRIBUTES lpSA, DWORD flProtect, DWORD dwMaxHigh, DWORD dwMaxLow, LPCSTR lpName) {
    return NULL;
}

HANDLE WINAPI CreateFileMappingXBoxW(HANDLE hFile, LPSECURITY_ATTRIBUTES lpSA, DWORD flProtect, DWORD dwMaxHigh, DWORD dwMaxLow, LPCWSTR lpName) {
    return NULL;
}

// Adaptador para FormatMessageA (Versión ANSI)
DWORD WINAPI FormatMessageXBoxA(
    DWORD dwFlags,
    LPCVOID lpSource,
    DWORD dwMessageId,
    DWORD dwLanguageId,
    LPSTR lpBuffer,
    DWORD nSize,
    va_list *Arguments
) {
    // Si SQLite pide el mensaje, le damos el código de error en texto
    if (lpBuffer && nSize > 25) {
        sprintf(lpBuffer, "XBox Error Code: %u", dwMessageId);
        return (DWORD)strlen(lpBuffer);
    }
    return 0;
}

// 1. Constante de protección de página faltante
#define PAGE_READWRITE 0x04

#ifndef SQLITE_DEBUG
	#ifdef __cplusplus
	extern "C" {
	#endif

	// La Xbox 360 no tiene ntdll.lib ni RtlValidateHeap. 
	// Devolvemos TRUE (1) para simular que el heap está bien.
	BOOL __stdcall RtlValidateHeap(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem) {
		return TRUE; 
	}

	// La Xbox 360 no tiene ntdll.lib ni RtlCompactHeap.
	// Devolvemos 0 (no se pudo compactar nada o no es necesario) para que SQLite continúe.
	SIZE_T __stdcall RtlCompactHeap(HANDLE hHeap, DWORD dwFlags) {
		return 0; 
	}

	#ifdef __cplusplus
	}
	#endif
#endif
#endif
