#include "dirutil.h"

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

dirutil::dirutil(){
}

dirutil::~dirutil(){
}

/**
 * 
 * @param ruta
 * @return 
 */
bool dirutil::isDir(const char* ruta){
    //struct stat info;
    //stat(ruta, &info);
    //
    //if(S_ISDIR(info.st_mode)){
    //    return true;
    //} else{
    //    return false;
    //}

    #ifdef DOS
        //Much faster for dos
        DIR* dir = opendir(ruta);
        bool ret = dir != NULL;
        if (ret)
            closedir(dir);
        return ret;
	//#elif defined(_XBOX)
	//	return GetFileAttributes(ruta) != 0xFFFFFFFF;
    #else 
        struct stat info;
        stat(ruta, &info);
        
        if(S_ISDIR(info.st_mode)){
            return true;
        } else{
            return false;
        }
    #endif
}

/**
 * Check if a file exists
 * @return true if and only if the file exists, false else
 */
bool dirutil::fileExists(const char* file) {
    //return std::filesystem::exists(file);

    #ifdef DOS
        //Much faster for dos
        ifstream f(file);
        bool ret = f.good();
        if (ret)
            f.close();
        return ret;
	#elif defined(_XBOX)
		return GetFileAttributes(file) != 0xFFFFFFFF;
    #else
        struct stat buf;
        return (stat(file, &buf) == 0);
    #endif
}

/**
* Comprueba si existe el directorio o fichero pasado por parametro
*/
bool dirutil::dirExists(const char* ruta){
    if(isDir(ruta)){
        return true;
    } else {
        return fileExists(ruta);
    }
}

/**
*
*/
char * dirutil::getDir(char *buffer){
	if (!buffer) return NULL;

#ifdef _XBOX
	memset(buffer, '\0', FILENAME_MAX-1);
	//Como no se puede obtener la ruta con el ejecutable, lo tenemos que harcodear
	//Hay que modificar Properties/Xbox360 image conversion/Output-file
	//y Properties/general/Target Name en windows
	//EMU_LIB_NAME es una macro que se puede modificar en el fichero Salvia.vcxproj
	string ruta = "game:\\"; 
	strncpy(buffer, ruta.c_str(), FILENAME_MAX);
    return buffer;
#else
	memset(buffer, '\0',FILENAME_MAX-1);
    return getcwd(buffer, PATH_MAX);
#endif
}

/**
*/
char * dirutil::getDirActual(){
    return getDir(rutaActual);
}


/**
* Obtiene la extensión del fichero (incluyendo el punto)
*/
string dirutil::getExtension(string file) {
    if (file.empty()) return "";

    // 1. Buscamos el último separador de carpeta para aislar el nombre
    size_t lastSep = file.find_last_of("/\\");
    size_t startSearch = (lastSep == string::npos) ? 0 : lastSep + 1;

    // 2. Buscamos el punto solo a partir del nombre del archivo
    size_t lastDot = file.find_last_of(".");

    // 3. Verificamos que el punto esté después del separador y no sea el primer carácter del nombre
    // (Esto evita archivos ocultos sin extensión como ".bashrc")
    if (lastDot != string::npos && lastDot >= startSearch && lastDot > 0) {
        string ext = file.substr(lastDot);
        Constant::lowerCase(&ext);
        return ext;
    }

    return "";
}


/**
 *
 * @param str
 * @param val
 * @return
 */
char* dirutil::formatdate(char* str, time_t val){
    int tam = 36;
    strftime(str, tam, "%d/%m/%y %H:%M", localtime(&val));
    return str;
}

bool dirutil::setFileProperties(FileProps *propFile, string ruta){
    bool ret = true;

    struct stat info;
    stat(ruta.c_str(), &info);

    if(S_ISDIR(info.st_mode)){
        propFile->filetype = TIPODIRECTORIO;
        propFile->extension = STR_DIR_EXT;
    } else {
        propFile->filetype = TIPOFICHERO;
        propFile->fileSize = (size_t)info.st_size;
        propFile->extension = getExtension(ruta);
    }
    char mbstr[36];
    propFile->creationTime = formatdate(mbstr, info.st_ctime);
    propFile->modificationTime = formatdate(mbstr, info.st_mtime);
    propFile->iCreationTime = time(&info.st_ctime);
    propFile->iModificationTime = time(&info.st_mtime);

    return ret;
}

int dirutil::findIcon(const char *filename){

    char ext[5] = {' ',' ',' ',' ','\0'};
    int len = 0;

    if (filename != NULL){
        len = strlen(filename);
        if ( len > 4){
            ext[3] = filename[len-1];
            ext[2] = filename[len-2];
            ext[1] = filename[len-3];
            ext[0] = filename[len-4];
        }
    }
    string data = ext;
    std::transform(data.begin(), data.end(), data.begin(), ::tolower);

    if (data.find(".txt") != string::npos || data.find(".inf") != string::npos){
        return page_white_text;
    } else if (data.find(".gpu") != string::npos || data.find(".gpe") != string::npos
        || data.find(".exe") != string::npos || data.find(".bat") != string::npos
        || data.find(".com") != string::npos){
        return page_white_gear;
    } else if (data.find(".gz") != string::npos || data.find(".z") != string::npos
        || data.find(".tar") != string::npos || data.find(".zip") != string::npos
        || data.find(".rar") != string::npos){
	    return page_white_compressed;
	} else if (data.find(".bmp") != string::npos || data.find(".jpg") != string::npos
        || data.find(".jpeg") != string::npos || data.find(".png") != string::npos
        || data.find(".gif") != string::npos ){
        return page_white_picture;
    } else if (data.find(".bin") != string::npos){
        return page_white_zip;
    } else {
        return page_white;
    }

}

/**
 *
 * @param strdir
 * @param filelist
 * @param filtro
 * @param superfast
 * @return
 */
unsigned int dirutil::listarFilesSuperFast(const char *strdir, vector<unique_ptr<FileProps>> &filelist, string filtro, bool order, bool properties){
    unsigned int totalFiles = 0;
	return listarFilesSuperFast(strdir, filelist, filtro, "", order, properties);
}


unsigned int dirutil::listarFilesSuperFast(const char *strdir, vector<unique_ptr<FileProps>> &filelist, string filtroExt, string filtroName, bool order, bool properties){
    unsigned int totalFiles = 0;

#ifdef _XBOX
	WIN32_FIND_DATA findData;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    // Es necesario añadir "\*" al final de la ruta para buscar todos los archivos
    std::string searchPath = strdir;
    if (searchPath[searchPath.length() - 1] != '\\') {
        searchPath += "\\";
    }
	string parentDir = searchPath;
    searchPath += "*";

    // Iniciar la búsqueda
    hFind = FindFirstFile(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        std::cout << "No se pudo abrir el directorio o está vacío: " << strdir << std::endl;
        return 0;
    }
	string extension;

    do {
        // Ignorar los directorios especiales "." y ".." si aparecen
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
            continue;
        }

        // Comprobar si es un directorio o un archivo
        /*if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::cout << "[DIR]  " << findData.cFileName << std::endl;
        } else {
            std::cout << "[FILE] " << findData.cFileName << std::endl;
        }*/

		string concatDir = parentDir + findData.cFileName;
		extension = getExtension(findData.cFileName);
        if (foundFilter(filtroExt, filtroName, extension, findData.cFileName)){
            FileProps propFile(strdir, findData.cFileName, findIcon(findData.cFileName), TIPOFICHERO);
            if (properties){
                setFileProperties(&propFile, concatDir);
                propFile.ico = findIcon(findData.cFileName);
                propFile.filetype = TIPOFICHERO;
            }
			filelist.emplace_back(std::unique_ptr<FileProps>(new FileProps(propFile)));
        }

    } while (FindNextFile(hFind, &findData) != 0);

    // Es fundamental cerrar el handle para evitar fugas de memoria
    FindClose(hFind);
#else
	DIR *dp;
    struct dirent *dirp;
    if (!dirExists(strdir))
        return 0;

    string parentDir = strdir;
    //Miramos a ver si el directorio a explorar tiene una / al final
    if (strdir != NULL){
		if (!parentDir.empty() && parentDir.at(parentDir.length()-1) != Constant::getFileSep()[0]){
            parentDir.append(Constant::tempFileSep);
        }
        string extension;

        if((dp  = opendir(strdir)) == NULL) {
            //Traza::print("Error al listar el directorio: " + string(strdir), W_ERROR);
            return 0;
        } else {
            //Traza::print("Recorriendo ficheros", W_PARANOIC);
            while ((dirp = readdir(dp)) != NULL) {
                string concatDir = parentDir + string(dirp->d_name);
                //if (!isDir(concatDir)){
                    extension = getExtension(dirp->d_name);
                    if (foundFilter(filtroExt, filtroName, extension, dirp->d_name)){
                        FileProps propFile(strdir, dirp->d_name, findIcon(dirp->d_name), TIPOFICHERO);
                        if (properties){
                            setFileProperties(&propFile, concatDir);
                            propFile.ico = findIcon(dirp->d_name);
                            propFile.filetype = TIPOFICHERO;
                        }
                        //filelist.emplace_back(make_unique<FileProps>(propFile));
						filelist.emplace_back(std::unique_ptr<FileProps>(new FileProps(propFile)));
                    }
                //}
            }
            closedir(dp);
        }
    }
#endif
	totalFiles = filelist.size();
    if (order && totalFiles > 0) {
        std::sort (filelist.begin(), filelist.end(), FileProps::sortByTextUnique);
    }
    return totalFiles;
}

bool dirutil::foundFilter(std::string filtroExt, std::string filtroName, std::string extension, std::string name){
	return (filtroExt.empty() && filtroName.empty()) ||
						 (!filtroExt.empty() && extension.length() > 1 && filtroExt.find(extension) != string::npos) ||
						 (!filtroName.empty() && name.find(filtroName) != string::npos);
}

string dirutil::getFileNameNoExt(string file) {
    if(isDir(file.c_str())) return file;

    // 1. Encontrar el último separador de carpeta (cualquiera de los dos)
    size_t lastSep = file.find_last_of("/\\");
    
    // 2. Extraer solo el nombre del archivo (con extensión)
    string fileName = (lastSep == string::npos) ? file : file.substr(lastSep + 1);
    
    // 3. Buscar el último punto solo en el nombre del archivo
    size_t lastDot = fileName.find_last_of(".");
    
    // 4. Si hay punto y no es el primer carácter (evita archivos ocultos tipo .bashrc)
    if (lastDot != string::npos && lastDot > 0) {
        return fileName.substr(0, lastDot);
    }
    
    return fileName;
}

string dirutil::getFolder(string file) {
    if (isDir(file.c_str())) {
        return file;
    }

    // Buscamos el último separador de cualquier tipo (\ o /)
    size_t found = file.find_last_of("/\\");

    if (found != string::npos) {
        // Si el separador está al inicio (ej. "/file"), devolvemos "/"
        if (found == 0) return file.substr(0, 1);
        
        // Devolvemos la ruta hasta el último separador (sin incluirlo)
        return file.substr(0, found);
    }

    // Si no hay separadores, es un archivo en el directorio actual
    return "."; // O return "" según prefieras representar el directorio local
}

/**
 * Obtiene el nombre del fichero (con extensión) de una ruta completa
 */
string dirutil::getFileName(string file) {
    if (isDir(file.c_str())) {
        return file;
    }

    // Buscamos la última aparición de CUALQUIER separador de carpeta
    size_t found = file.find_last_of("/\\");

    if (found != string::npos) {
        // Extraemos todo lo que hay después del último separador
        return file.substr(found + 1);
    }

    // Si no hay separadores, el string ya es el nombre del archivo
    return file;
}

/**
* Devuelve true si se ha hecho el cambio al directorio.
* False si no se ha podido hacer el cambio. P.ejm: Cambio por un fichero
*/
bool dirutil::changeDirAbsolute(const char *str){
	#ifdef _XBOX
		return false;
	#else
		if(isDir(str)){
			return (chdir(str) != -1);
		} else {
			return false;
		}
	#endif
}

/**
*
*/
bool dirutil::borrarArchivo(string ruta){

    if (isDir(ruta.c_str()))
        return false;
    else {
        if (fileExists(ruta.c_str()))
            return (remove(ruta.c_str()) != 0) ? false : true;
        else
            return false;
    }
}

/**
*
*/
int dirutil::createDir(std::string dir) {
    if (dir.empty()) return 0;

    // 1. Normalizar barras para la plataforma (XDK es estricto con \)
    #if defined(_XBOX) || defined(_WIN32)
        char sep = '\\';
        for (size_t i = 0; i < dir.length(); ++i) if (dir[i] == '/') dir[i] = sep;
    #else
        char sep = '/';
        for (size_t i = 0; i < dir.length(); ++i) if (dir[i] == '\\') dir[i] = sep;
    #endif

    // 2. Creación recursiva (asegura que existan los padres)
    for (size_t i = 0; i < dir.length(); ++i) {
        if (dir[i] == sep && i > 0) {
            std::string sub = dir.substr(0, i);
            // Ignorar si es la letra de unidad (ej: "Hdd:")
            if (sub.find(':') == sub.length() - 1) continue; 
            
            #ifdef _XBOX
                CreateDirectoryA(sub.c_str(), NULL);
            #elif defined(_WIN32)
                _mkdir(sub.c_str());
            #else
                mkdir(sub.c_str(), 0777);
            #endif
        }
    }

    // 3. Crear el directorio final
    #ifdef _XBOX
        if (CreateDirectoryA(dir.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
            return 1; // Éxito o ya existía
        }
        return 0; // Error real (ej: disco lleno o protegido)
    #elif defined(_WIN32)
        return (_mkdir(dir.c_str()) == 0 || errno == EEXIST) ? 1 : 0;
    #else
        return (mkdir(dir.c_str(), 0777) == 0 || errno == EEXIST) ? 1 : 0;
    #endif
}

int dirutil::createDirRecursive(const char* path) {
    char temp[MAX_PATH];
    const char* p = path;
    
    // Saltamos el prefijo de la unidad (ej: "game:\", "hdd:\")
    if (strstr(path, ":\\")) {
        p = strstr(path, ":\\") + 2;
    }

	while ((p = strchr(p, Constant::tempFileSep[0])) != NULL) {
        size_t len = p - path;
        memcpy(temp, path, len);
        temp[len] = '\0';
        
        // Intentar crear el directorio intermedio
		#if defined(WIN) || defined(_XBOX)
			CreateDirectory(temp, NULL);
		#else 
			return mkdir(temp, 0777);
		#endif
        p++;
    }
    
	// Crear el directorio final
	#if defined(WIN) || defined(_XBOX)
		return CreateDirectory(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
	#else
		return mkdir(path, 0777);
	#endif
}

void dirutil::borrarDir(string path)
{
#ifdef WIN
    DIR *dir = opendir(path.c_str());
    if (dir == NULL) return; // Error al abrir o no es un directorio

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // Saltar "." y ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construir ruta completa
        string abs_path = path + "/" + entry->d_name;

        // Intentar abrir como directorio para ver si es subcarpeta
        DIR *sub_dir = opendir(abs_path.c_str());
        if (sub_dir != NULL) 
        {
            closedir(sub_dir);       // Cerramos el test de apertura
            borrarDir(abs_path);     // Llamada recursiva
        }
        else 
        {
            remove(abs_path.c_str()); // Es un archivo, borrar directamente
        }
    }

    closedir(dir);            // <--- IMPRESCINDIBLE
    rmdir(path.c_str());      // Borrar la carpeta actual ahora que está vacía
#elif defined(_XBOX)
	// En Xbox 360, las rutas deben terminar en \* para buscar contenido

	std::string searchPath = path;
    if (!searchPath.empty() && searchPath.at(searchPath.length() - 1) != '\\') {
        searchPath += "\\";
    }
    std::string baseWithSlash = searchPath; // Guardamos para concatenar rápido
    searchPath += "*";

    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        // En Xbox, remove() es más lento que DeleteFileA
        DeleteFileA(path.c_str());
        return;
    }

    do {
        const char* name = findData.cFileName;

        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
            continue;
        }

        std::string fullPath = baseWithSlash + name;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            borrarDir(fullPath);
        } else {
            // CRÍTICO: La Xbox 360 a veces marca archivos como READONLY si vienen de un DVD/ISO
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
                SetFileAttributesA(fullPath.c_str(), FILE_ATTRIBUTE_NORMAL);
            }
            DeleteFileA(fullPath.c_str());
        }
    } while (FindNextFile(hFind, &findData));

    FindClose(hFind);
    
    // Al eliminar el directorio, el sistema ya garantiza la consistencia
    RemoveDirectoryA(path.c_str());
#endif
}