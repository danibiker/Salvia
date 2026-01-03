#ifndef FILEIO_H_INCLUDED
#define FILEIO_H_INCLUDED

#include <fstream>
#include <string>
#include <iostream>

using namespace std;

class Fileio{
    public :
        Fileio();
        ~Fileio();
        bool loadFromMem(const unsigned char *, std::size_t tam);
        char * getFile() {return memblock;}
		ifstream::pos_type getFileSize() {return size;}
		bool clearFile();
    private:
        ifstream::pos_type size;
        char * memblock;
};

Fileio::Fileio(){
    memblock = NULL;
    size = 0;
}

Fileio::~Fileio(){
    clearFile();
}

bool Fileio::loadFromMem(const unsigned char *buf, std::size_t tam){
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

#endif // FILEIO_H_INCLUDED