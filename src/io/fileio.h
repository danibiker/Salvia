#ifndef FILEIO_H_INCLUDED
#define FILEIO_H_INCLUDED

#include <fstream>
#include <string>
#include <iostream>

class Fileio{
    public :
        Fileio();
        ~Fileio();
        bool loadFromMem(const unsigned char *, std::size_t tam);
        char * getFile() {return memblock;}
		std::ifstream::pos_type getFileSize() {return size;}
		bool clearFile();
		int writeToFile(const char *uri, char * memblocktowrite, size_t tam, int append);
    private:
		void decodeError(int r);
        std::ifstream::pos_type size;
        char * memblock;
};

#endif // FILEIO_H_INCLUDED