#include "fileio.h"
#include <sstream>

Fileio::Fileio(){
    memblock = NULL;
    size = 0;
}

Fileio::~Fileio(){
    clearFile();
}

bool Fileio::loadFromMem(const unsigned char *buf, std::size_t tam){
	if (memblock != NULL){
		clearFile();
	}
    memblock = new char [tam];//Reservamos memoria
    memcpy ( memblock, buf, tam );
    size = tam;
    return true;
}

bool Fileio::clearFile(){
    if (memblock != NULL){
        size = 0;
        delete [] memblock;
        memblock = NULL;
        return true;
    }
    return false;
}

std::string Fileio::cargarFichero(const std::string& ruta) {
    // Abrimos el flujo de entrada (ifstream)
    std::ifstream archivo(ruta.c_str()); 
    
    // Verificación de apertura (estándar en VS2010)
    if (!archivo.is_open()) {
        return ""; 
    }

    // Usamos stringstream como buffer intermedio
    std::stringstream buffer;
    buffer << archivo.rdbuf(); // Vuelca todo el contenido del fichero al buffer

    // Cerramos el archivo y devolvemos la cadena
    archivo.close();
    return buffer.str();
}

/**
* Writes the specified amount of data of the memory buffer to disk
*
* ios::in	Open for input operations.
* ios::out	Open for output operations.
* ios::binary	Open in binary mode.
* ios::ate	Set the initial position at the end of the file. If this flag is not set to any value, the initial position is the beginning of the file.
* ios::app	All output operations are performed at the end of the file, appending the content to the current content of the file. This flag can only be used in streams open for output-only operations.
* ios::trunc	If the file opened for output operations already existed before, its previous content is deleted and replaced by the new one.
*/
int Fileio::writeToFile(const char *uri, char * memblocktowrite, size_t tam, int append){
    int ret = 0;
    FILE *file;
    errno_t err;

    err = fopen_s(&file, uri, "wb");

    if (err == 0 && file != NULL){
        if (append) {
            fseek(file, 0, SEEK_END);
        }
        fwrite(memblocktowrite, 1, tam, file);
        fclose(file);
        ret = 1;
        printf("writeToFile: downloaded file in: %s\n", uri);
    } else {
        printf("writeToFile: file: %s not found or could not be opened", uri);
        decodeError(err);
    }
    #ifdef GP2X
        sync();
    #endif
    return ret;
}

void Fileio::decodeError(int r){
    char buff[100];
    strerror_s(buff, 100, r);
    printf("str_trim_left.error: %d %s\n", r, buff);
}