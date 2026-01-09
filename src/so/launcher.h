#pragma once

#include "const/Constant.h"
#include "beans/structures.h"
#include "font/fonts.h"
#include "io/dirutil.h"

#ifdef WIN
	#include <shellapi.h>
#elif defined(_XBOX)
	#include <xbox.h>
	#include <xtl.h>
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

#include <string>
#include <fstream>
#include <iostream>
#include <string.h>

using namespace std;

class Launcher{
    public:
        Launcher(){};
        ~Launcher(){
           
        };
        bool lanzarProgramaUNIXFork(FileLaunch &emulInfo);
        bool launch(vector<string> &commands, bool debug);
        string descomprimirZIP(string filename);
    private:
        Executable rutaEspecial(string ejecutable, string param, string filerompath);
        void deleteUnzipedRom(string romfile);
        int dosbatch(vector<string> &commands, string comando, bool debug);
        string getBatchPath();
		void lanzarEnSuCarpeta(const std::string& rutaCompletaExe, const std::string& parametros);
};  

/**
 * 
 */
Executable Launcher::rutaEspecial(string ejecutable, string param, string filerompath){
    Executable cmdExec;
    cmdExec.filenameinparms = false;
    dirutil dir;

    if (param.find("%") != std::string::npos){

        if (param.find("%ROMNAME%") !=std::string::npos || param.find("%ROMNAMEXT%") !=std::string::npos
            || param.find("%ROMFULLPATH%") !=std::string::npos){
            cmdExec.filenameinparms = true;
        }

        param = Constant::replaceAll(param, "%ROMPATH%", dir.getFolder(filerompath));
        param = Constant::replaceAll(param, "%ROMNAME%", dir.getFileNameNoExt(filerompath));
        param = Constant::replaceAll(param, "%ROMNAMEXT%", dir.getFileName(filerompath));
        param = Constant::replaceAll(param, "%ROMFULLPATH%", filerompath);

        if (cmdExec.filenameinparms){
            cmdExec.comandoFinal = ejecutable + " " + param;
        } else {
            cmdExec.comandoFinal = ejecutable + " " + param + " " + filerompath;
        }
    } else {
        if (filerompath.empty())
            filerompath = "";
        else
            filerompath = "\"" + filerompath + "\"";

        cmdExec.comandoFinal = ejecutable + " " + param + " " + filerompath ;
    }
    cout << "Launcher::launch. comandoFinal: " + cmdExec.comandoFinal <<endl;

    cmdExec.ejecutable = ejecutable;
    cmdExec.param = param;
    cmdExec.filerompath = filerompath;

    return cmdExec;
}

/**
 * 
 */
bool Launcher::launch(vector<string> &commands, bool debug){
    bool launchOk = false;
    string comando;
    //#ifdef WIN
    //    comando = "start ";
    //#endif

    //clear_to_color(screen, Constant::backgroundColor);
    //ALFONT_FONT *fontsmall = Fonts::getFont(Fonts::FONTSMALL);
    //Constant::drawTextCent(screen, fontsmall, "launching command", SCREEN_W / 2, SCREEN_H / 2, Constant::textColor, -1);

    for (std::size_t i=0; i < commands.size(); i++){
        string parm = Constant::Trim(commands.at(i));
        if (!parm.empty()){
            comando += (i > 0 ? " " : "") + parm;
        } 
        //if (debug){
        //    Constant::drawTextCent(screen, fontsmall, parm.c_str(), SCREEN_W / 2, SCREEN_H / 2 + (fontsmall->face_h + 3) * (i+1), Constant::textColor, -1);
        //}
    }

    //if (debug){
    //    Constant::drawTextCentre(screen, fontsmall, comando.c_str(), SCREEN_W / 2, SCREEN_H / 2 + (fontsmall->face_h + 3) * (commands.size()+1), Constant::textColor, -1);
    //    Constant::drawTextCentre(screen, fontsmall, "Press a key to continue", SCREEN_W / 2, SCREEN_H / 2 + (fontsmall->face_h + 3) * (commands.size()+2), Constant::textColor, -1);
    //    readkey();
    //}

    if (Constant::getExecMethod() == launch_system ){
		#ifdef WIN
			LOG_DEBUG("Launching command -> %s\n", comando.c_str());
			launchOk = system(comando.c_str()) == 0;
			LOG_DEBUG("Comando terminado\n");
		#endif
    } else if (Constant::getExecMethod() == launch_spawn || Constant::getExecMethod() == launch_create_process){
        LOG_DEBUG("Launching command -> %s\n", comando.c_str());
        // extra room for program name and sentinel
        char **argv = new char* [commands.size()+1];  
        std::size_t j = 0;
        std::size_t posDirSep;
        LOG_DEBUG("Launching command:\n");

        for (; j < commands.size(); j++){
            if (j==0 && ((posDirSep = commands[j].find_last_of(Constant::tempFileSep)) != string::npos)){
                argv[j] = strdup(commands[j].substr(posDirSep + 1).c_str());
            } else {
                argv[j] = strdup(commands[j].c_str());
            }
			LOG_DEBUG("param_%d=%s\n", j, argv[j]);
        }     
        // end of arguments sentinel is NULL
        argv [j] = NULL;  
        
        #ifdef UNIX
            if ( fork() == 0 ){
                //by convention, argv[0] is the full program path
                if (execv(commands[0].c_str(), argv) == -1){
                    Traza::print(Traza::T_ERROR, "No se ha podido ejecutar el programa");
                    return false;
                } else {
                    return true;
                }
            } else {
                int ret;
                wait(&ret); 
                Traza::print(Traza::T_DEBUG, "Comando terminado");
            }        
        #elif defined(WIN) || defined(_XBOX)
			lanzarEnSuCarpeta(commands[0], commands[1]);
			/*
			// 1. Obtener la carpeta del nuevo EXE
			std::string newDir = commands[0].substr(0, commands[0].find_last_of("\\/"));

			// 2. Cambiar el directorio de trabajo del proceso actual
			// El proceso que nazca con execv heredará este nuevo directorio
			if (chdir(newDir.c_str()) == 0) {
				if (execv(commands[0].c_str(), argv) == -1){
					LOG_ERROR("No se ha podido ejecutar el programa");
					return false;
				} else {
					LOG_DEBUG("Comando terminado");
					return true;
				}
			} else {
				// Error al cambiar de carpeta
			}*/
        #endif 

        // Deallocate memory
        for (j = 0; j < commands.size(); j++)     
            free(argv[j]);
        delete [] argv;

    } else if (Constant::getExecMethod() == launch_batch && dosbatch(commands, comando, debug) == 0){
        LOG_DEBUG("Launching command -> %s", comando.c_str());
        exit(0);
    }
    return launchOk;
}

void Launcher::lanzarEnSuCarpeta(const std::string& rutaCompletaExe, const std::string& parametros) {
	#ifdef WIN
		// Extraer la carpeta del ejecutable
		std::string directory = rutaCompletaExe.substr(0, rutaCompletaExe.find_last_of("\\/"));

		SHELLEXECUTEINFOA sei = {0};
		sei.cbSize = sizeof(SHELLEXECUTEINFOA);
		sei.lpVerb = "open";
		sei.lpFile = rutaCompletaExe.c_str();
		sei.lpParameters = parametros.c_str();
		sei.lpDirectory = directory.c_str(); // <--- ESTO fija el directorio de trabajo
		sei.nShow = SW_SHOWNORMAL;

		if (ShellExecuteExA(&sei)) {
			exit(0); // Salida limpia
		}
	#elif defined(_XBOX)
		// 2. Establecer los datos de lanzamiento (Argumentos)
		// El sistema espera un puntero a los datos y el tamaño total.
		// Pasamos la ruta de la ROM para que RetroArch la reciba.
		XSetLaunchData((PVOID)parametros.c_str(), (DWORD)parametros.length() + 1);

		// Montamos el dispositivo físico en el alias "Usb0:"
		// El '1' indica que es un montaje de lectura/escritura (falso para sólo lectura)
		if (GetFileAttributes(rutaCompletaExe.c_str()) != 0xFFFFFFFF){
			XLaunchNewImage(rutaCompletaExe.c_str(), 0);
		}
		exit(0); // Salida limpia
	#endif
}

int Launcher::dosbatch(vector<string> &commands, string comando, bool debug){
    
    dirutil dir;        
    std::fstream file(getBatchPath(), std::ios::out);

    if (!file) {
        std::cerr << "Error opening file!" << std::endl;
        return 1; // or handle the error as needed
    }

    if (debug) file << "@echo on" <<endl;
    else file << "@echo off" <<endl;

    file << "cd " << dir.getFolder(commands.at(0)) << endl;
    if (debug) {
        file << "echo will run: \"" + comando + "\"" << endl;
        file << "pause" << endl;
    }
    file << comando << endl;
    file << "cd " << Constant::getAppDir() << endl;
    
    //if (debug) file << "pause" << endl;
    //file << Constant::getAppDir() << Constant::getFileSep() << dir.getFileName(argv0) << endl;
    
    file.close();
    return 0;
}

string Launcher::getBatchPath(){
    return Constant::getAppDir() //+ Constant::getFileSep() + "gmenu" 
        + Constant::getFileSep() + "run.bat";
}