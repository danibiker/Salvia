#include "launcher.h"

#ifdef _XBOX
	#include <xbox.h>
	#include <xtl.h>

	extern "C" {
		// Estructura necesaria para las llamadas al Kernel
		struct XBOX_ANSI_STRING {
			USHORT Length;
			USHORT MaximumLength;
			PSTR Buffer;
		};
		LONG WINAPI ObCreateSymbolicLink(XBOX_ANSI_STRING* LinkName, XBOX_ANSI_STRING* DeviceName);
		LONG WINAPI ObDeleteSymbolicLink(XBOX_ANSI_STRING* LinkName);
	}
#elif defined WIN
	#include <shellapi.h>
#endif

vector<string> Launcher::mountedDrives;

bool Launcher::launch(std::vector<std::string> &commands){
   	return launchXboxWin(commands[0], commands[1]) == 0;
}

int Launcher::launchXboxWin(const std::string& rutaCompletaExe, const std::string& parametros) {
	LOG_DEBUG("Launching %s %s", rutaCompletaExe.c_str(), parametros.c_str());
	
	#ifdef _XBOX
		if (GetFileAttributes(rutaCompletaExe.c_str()) != 0xFFFFFFFF){
			if (!parametros.empty())
				XSetLaunchData((PVOID)parametros.c_str(), (DWORD)parametros.length() + 1);
			XLaunchNewImage(rutaCompletaExe.c_str(), 0);
			return 0;
		}
	#elif defined(WIN)
		dirutil dir;
		// Extraer la carpeta del ejecutable
		std::string directory = rutaCompletaExe.substr(0, rutaCompletaExe.find_last_of("\\/"));

		SHELLEXECUTEINFOA sei = {0};
		sei.cbSize = sizeof(SHELLEXECUTEINFOA);
		sei.lpVerb = "open";
		sei.lpFile = rutaCompletaExe.c_str();
		sei.lpParameters = parametros.c_str();
		sei.lpDirectory = directory.c_str();
		sei.nShow = SW_SHOWNORMAL;

		if (dir.fileExists(rutaCompletaExe.c_str()) && ShellExecuteExA(&sei)) {
			//Hay que hacer un exit porque ya se ha abierto un nuevo programa
			//y hay que detener el flujo del actual en windows
			exit(0);
		}
	#endif
	return 1;
}

void Launcher::mount(const char* szDrive, const char* szDevice) {
    #ifdef _XBOX

	auto it = std::find(mountedDrives.begin(), mountedDrives.end(), szDrive);
    if (it != mountedDrives.end()) {
		//if it's already mounted, just do nothing
		return;
	} else {
		//if it isn't mounted, add it to the list and continue
		mountedDrives.push_back(szDrive);
	}


    XBOX_ANSI_STRING linkName, deviceName; // XBOX_ANSI_STRING es equivalente a STRING
    char linkPath[260];

    // Formato correcto para el gestor de objetos: \??\Nombre:
    // Es vital que el destino tenga el prefijo \??\ para que sea global
    _snprintf(linkPath, sizeof(linkPath), "\\??\\%s", szDrive);

    // Inicialización de estructuras del Kernel
    linkName.Buffer = linkPath;
    linkName.Length = (USHORT)strlen(linkPath);
    linkName.MaximumLength = (USHORT)(linkName.Length + 1);

    deviceName.Buffer = (PSTR)szDevice;
    deviceName.Length = (USHORT)strlen(szDevice);
    deviceName.MaximumLength = (USHORT)(deviceName.Length + 1);

    // Creamos el enlace simbólico
    ObCreateSymbolicLink(&linkName, &deviceName);
    #endif
}

void Launcher::unmount(const char* szDrive) {
#ifdef _XBOX
    
	auto it = std::find(mountedDrives.begin(), mountedDrives.end(), szDrive);
    if (it != mountedDrives.end()) {
		//If it was mounted, delete from the list and continue
		mountedDrives.erase(it);
	} else {
		return;
	}
	
	XBOX_ANSI_STRING linkName;
    char linkPath[260];

    _snprintf(linkPath, sizeof(linkPath), "\\??\\%s", szDrive);

    linkName.Buffer = linkPath;
    linkName.Length = (USHORT)strlen(linkPath);
    linkName.MaximumLength = (USHORT)(linkName.Length + 1);

    ObDeleteSymbolicLink(&linkName);
#endif
}

void Launcher::initDrives() {
    #ifdef _XBOX
    // Unidades de Disco y USB
    mount("Hdd:",    "\\Device\\Harddisk0\\Partition1");
    mount("Usb0:",   "\\Device\\Mass0");
    mount("Usb1:",   "\\Device\\Mass1");
    mount("Usb2:",   "\\Device\\Mass2");
    
    // Unidades de Memoria y Sistema
    //mount("MemUnit0:","\\Device\\Mu0");      // Unidad de memoria A
    //mount("MemUnit1:","\\Device\\Mu1");      // Unidad de memoria B
    //mount("IntMu:",   "\\Device\\BuiltInMuSfc"); // Memoria interna 4GB (Slim)
    
    // El CD/DVD es fundamental si el emulador se quema en un disco
    //mount("Cd:",     "\\Device\\Cdrom0");
    #endif
}

void Launcher::unmountAll() {
	#ifdef _XBOX
	unmount("Hdd:");
    unmount("Usb0:");
    unmount("Usb1:");
    unmount("Usb2:");
	#endif	
}