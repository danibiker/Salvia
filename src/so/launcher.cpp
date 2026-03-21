#include "launcher.h"

vector<string> Launcher::mountedDrives;

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

	for (std::size_t i=0; i < commands.size(); i++){
        string parm = Constant::Trim(commands.at(i));
        if (!parm.empty()){
            comando += (i > 0 ? " " : "") + parm;
        } 
    }

    if (Constant::getExecMethod() == launch_system ){
		#ifdef WIN
			LOG_DEBUG("Launching command -> %s", comando.c_str());
			launchOk = system(comando.c_str()) == 0;
			LOG_DEBUG("Comando terminado");
		#endif
    } else if (Constant::getExecMethod() == launch_spawn || Constant::getExecMethod() == launch_create_process){
        LOG_DEBUG("Launching command -> %s", comando.c_str());
         #ifdef UNIX
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
			// Deallocate memory
			for (j = 0; j < commands.size(); j++)     
				free(argv[j]);
			delete [] argv;
        #elif defined(WIN) || defined(_XBOX)
			launchOk = launchFromDirectory(commands[0], commands[1]) == 0;
        #endif 
    } 
	//else if (Constant::getExecMethod() == launch_batch && dosbatch(commands, comando, debug) == 0){
    //    LOG_DEBUG("Launching command -> %s", comando.c_str());
    //    return false;
    //}
    return launchOk;
}

int Launcher::launchFromDirectory(const std::string& rutaCompletaExe, const std::string& parametros) {
	LOG_DEBUG("Launching %s %s", rutaCompletaExe.c_str(), parametros.c_str());
	
	#ifdef _XBOX
		if (GetFileAttributes(rutaCompletaExe.c_str()) != 0xFFFFFFFF){
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
		sei.lpDirectory = directory.c_str(); // <--- ESTO fija el directorio de trabajo
		sei.nShow = SW_SHOWNORMAL;

		if (dir.fileExists(rutaCompletaExe.c_str()) && ShellExecuteExA(&sei)) {
			exit(0);
		}
	#endif

	return 1;
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