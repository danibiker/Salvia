#include <io/joymapper.h>
#include <io/filelist.h>
#include <vector>

int *JoyMapper::joyButtonMapper;
std::string JoyMapper::rutaIni;

JoyMapper::JoyMapper(){
}

JoyMapper::~JoyMapper(){
    clearJoyMapper();
}

/**
*
*/
void JoyMapper::saveJoyConfig(){
    if (joyButtonMapper != NULL && !rutaIni.empty()){
		std::vector<std::string> fileConfigJoystick;

        std::string fila = "";
        for (int i = 0; i < MAXJOYBUTTONS; i++){
            fila = "JOY_" + Constant::TipoToStr(i) + "=" + Constant::TipoToStr(joyButtonMapper[i]);
			fileConfigJoystick.push_back(fila);
        }
		FileList::guardarVector(rutaIni, fileConfigJoystick);
    }
}

/**
*
*/
bool JoyMapper::initJoyMapper(){
    joyButtonMapper = NULL;
    rutaIni = Constant::getAppDir() + Constant::getFileSep() + "joystick.ini";
	std::vector<std::string> fileConfigJoystick;
	FileList::cargarVector(rutaIni, fileConfigJoystick);
	bool loadDefaults = true;

    if (joyButtonMapper == NULL){
        joyButtonMapper = new int[MAXJOYBUTTONS];
    }

    if (fileConfigJoystick.size() > 0){
        std::string linea = "";
        for (unsigned int i=0; i<fileConfigJoystick.size(); i++){
            linea = fileConfigJoystick.at(i);
            if (i < MAXJOYBUTTONS){
                int valorIni = Constant::strToTipo<int>(Constant::split(linea, "=").at(1));
                joyButtonMapper[i] = valorIni;
            }
        }
		loadDefaults = false;
    } 

	int num_elementos = sizeof(defaultButtons) / sizeof(defaultButtons[0]);
	if (loadDefaults){
        for (int i = 0; i < MAXJOYBUTTONS && i < num_elementos; i++){
            joyButtonMapper[i] = defaultButtons[i];
        }
    }

	return !loadDefaults;
}

/**
*/
int JoyMapper::getJoyMapper(int button){
    if (joyButtonMapper != NULL){
        if (button < MAXJOYBUTTONS && button >= 0){
            return joyButtonMapper[button];
        }
    }
    return -1;
}

void JoyMapper::setJoyMapper(int button, int value){
    if (joyButtonMapper != NULL){
        if (button < MAXJOYBUTTONS && button >= 0){
            joyButtonMapper[button] = value;
        }
    }
}

/**
*
*/

void JoyMapper::clearJoyMapper(){
    if (joyButtonMapper != NULL){
        delete [] joyButtonMapper;
        joyButtonMapper = NULL;
    }
}

