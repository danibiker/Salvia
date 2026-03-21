#pragma once

#include "const/Constant.h"
#include "beans/structures.h"
#include "font/fonts.h"
#include "io/dirutil.h"
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <string.h>

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
#else
	#ifdef __GNUC__
	#include <unistd.h>
	#else
	#include <process.h>
	#endif

	#if defined(_POSIX_VERSION)
		#include <sys/wait.h>
	#endif
#endif

using namespace std;

class Launcher{
    public:
        Launcher(){};
        ~Launcher(){
           
        };
        bool lanzarProgramaUNIXFork(FileLaunch &emulInfo);
        bool launch(vector<string> &commands, bool debug);
        string descomprimirZIP(string filename);
		static void initDrives();
		static void unmountAll();
    private:
        Executable rutaEspecial(string ejecutable, string param, string filerompath);
        void deleteUnzipedRom(string romfile);
        int dosbatch(vector<string> &commands, string comando, bool debug);
        string getBatchPath();
		int launchFromDirectory(const std::string& rutaCompletaExe, const std::string& parametros);
		static void mount(const char* szDrive, const char* szDevice);
		static void unmount(const char* szDrive);

		static vector<string> mountedDrives;
};  

