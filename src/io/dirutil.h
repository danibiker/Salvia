#pragma once

#include <stdio.h>

#ifdef _MSC_VER
	#include <direct.h>
	#ifndef chdir
		#define chdir _chdir
	#endif
	#ifndef getcwd
		#define getcwd _getcwd
	#endif

	#define mkdir(path) _mkdir(path)
		
	#ifdef _XBOX
		#include <xtl.h>		
		#define PATH_MAX MAX_PATH
		
		#ifndef S_ISDIR
			#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
		#endif

		#ifndef S_ISREG
			#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
		#endif
	#else
		#include "../compat/dirent.h"
	#endif
#else
	#include <dirent.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include <memory>
//#include <filesystem>
#include <fstream>

#include <io/fileprops.h>

#define STR_DIR_EXT "Dir"

class dirutil{
    public :
        dirutil();
        ~dirutil();
        char * getDir(char *buffer);
        char * getDirActual();
        string getExtension(string file);
        bool setFileProperties(FileProps *propFile, string ruta);
        unsigned int listarFilesSuperFast(const char *strdir, vector<unique_ptr<FileProps>> &filelist, string filtro, bool order, bool properties);
		unsigned int listarFilesSuperFast(const char *strdir, vector<unique_ptr<FileProps>> &filelist, string filtroExt, string filtroName, bool order, bool properties);
        string getFileNameNoExt(string file);
        string getFolder(string file);
        string getFileName(string file);
        bool changeDirAbsolute(const char *str);
        bool borrarArchivo(string ruta);
		void borrarDir(string path);
		int createDir(std::string dir);
		int createDirRecursive(const char* path);
		static std::string getPathPrefix(std::string filepath, std::string basePath = "");
		static bool fileExists(const char* file);
		static bool dirExists(const char* ruta);
		static bool isDir(const char* ruta);
    private:
        char rutaActual[PATH_MAX]; //Ruta actual que se esta navegando
        char* formatdate(char* str, time_t val);
        int findIcon(const char *filename);
		bool foundFilter(std::string filtroExt, std::string filtroName, std::string extension, std::string name);

};

