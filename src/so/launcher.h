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

class Launcher{
    public:
        Launcher(){};
        ~Launcher(){};

        bool launch(std::vector<std::string> &commands);
		int launchXboxWin(const std::string& rutaCompletaExe, const std::string& parametros);
		static void initDrives();
		static void unmountAll();
    private:
		static void mount(const char* szDrive, const char* szDevice);
		static void unmount(const char* szDrive);
		static vector<string> mountedDrives;
};  

