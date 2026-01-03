#include "fileio.h"

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