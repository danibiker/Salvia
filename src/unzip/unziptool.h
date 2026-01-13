#pragma once 

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>

#undef Z_HAVE_UNISTD_H
#include <unzip/minizip-1.2.5/unzip.h>
#include <unzip/zlib.h>

#include "io/fileio.h"
#include "io/dirutil.h"
#include "io/fileprops.h"
#include "const/constant.h"

using namespace std;

#define MAX_FILENAME 512

struct unzippedFileInfo{
    int errorCode;
    string rutaEscritura;
    std::size_t romsize;
    uint32_t nFilesInZip;
    uint32_t nFilesWritten;
    vector<FileProps> files;
};

class UnzipTool
{
    public:
        UnzipTool();
        virtual ~UnzipTool();
        unzippedFileInfo descomprimirZip(const char *);
        unzippedFileInfo descomprimirZip(const char *rutaZip, const char *rutaDest);
		unzippedFileInfo descomprimirZip(const char *rutaZip, const char *rutaDest, const char *forcedFileName);
		unzippedFileInfo descomprimirZip(const char *rutaZip, const char *rutaDest, const char *forcedFilename, bool uncompress);
        unzippedFileInfo descomprimirZip(const char *rutaZip, const char *rutaDest, bool uncompress);
        unzippedFileInfo listZipContent(const char *rutaZip);
		unzippedFileInfo descomprimirZipToMem(std::string, std::string, void*&);
		
    protected:
    private:
		unzippedFileInfo extraerFichero(unzFile *myZip, const char *ruta, const char *forcedFilename, bool uncompress);
        unzippedFileInfo extraerFichero(unzFile *myZip, const char *ruta, bool uncompress);
        unzippedFileInfo extraerFichero(unzFile *myZip, const char *ruta);
		unzippedFileInfo extraerFicheroToMem(unzFile *, std::string, int, void*&);

};

UnzipTool::UnzipTool()
{
    //ctor
}

UnzipTool::~UnzipTool()
{
    //dtor
}

unzippedFileInfo UnzipTool::descomprimirZip(const char *ruta){
    return descomprimirZip(ruta, ruta);
}

unzippedFileInfo UnzipTool::descomprimirZip(const char *rutaZip, const char *rutaDest){
    return descomprimirZip(rutaZip, rutaDest, true);
}

unzippedFileInfo UnzipTool::descomprimirZip(const char *rutaZip, const char *rutaDest, const char *forcedFileName){
    return descomprimirZip(rutaZip, rutaDest, forcedFileName, true);
}

unzippedFileInfo UnzipTool::listZipContent(const char *rutaZip){
    return descomprimirZip(rutaZip, rutaZip, false);
}

unzippedFileInfo UnzipTool::descomprimirZip(const char *rutaZip, const char *rutaDest, bool uncompress){
    return descomprimirZip(rutaZip, rutaZip, NULL, uncompress);
}

unzippedFileInfo UnzipTool::extraerFichero(unzFile *myZip, const char *ruta){
    return extraerFichero(myZip, ruta, true);
}

unzippedFileInfo UnzipTool::extraerFichero(unzFile *myZip, const char *ruta, bool uncompress){
   return extraerFichero(myZip, NULL, uncompress);
}

unzippedFileInfo UnzipTool::descomprimirZip(const char *rutaZip, const char *rutaDest, const char *forcedFilename, bool uncompress){
	unzFile myZip = NULL;
    myZip = unzOpen(rutaZip);
    unz_global_info zip_global_info;
    int ret;
    dirutil dir;

    unsigned long nFilesInZip = 0;
    unzippedFileInfo retorno;
    unzippedFileInfo tempRetorno;

    tempRetorno.errorCode = -1;
    tempRetorno.rutaEscritura = rutaZip;
    tempRetorno.romsize = 0;
    tempRetorno.nFilesInZip = 0;
    tempRetorno.nFilesWritten = 0;
    
    retorno.errorCode = -1;
    retorno.rutaEscritura = rutaZip;
    retorno.romsize = 0;
    retorno.nFilesInZip = 0;
    retorno.nFilesWritten = 0;


    if (myZip == NULL){
        cerr << "UnzipTool::descomprimirZip. Unable to open zip file: " + string(rutaZip) << endl;
        return(retorno);
    }
    else{
        cout << "UnzipTool::descomprimirZip. File found: " + string(rutaZip) << endl;
    }

    ret = unzGetGlobalInfo(myZip, &zip_global_info);
    if (ret != UNZ_OK){
        cerr << "unzGetGlobalInfo() call failed." + nFilesInZip << endl;
        return(retorno);
    } else {
        nFilesInZip = zip_global_info.number_entry;
        tempRetorno.nFilesInZip = nFilesInZip;
        cout << "Found files in archive." + nFilesInZip <<endl;
    }

    ret = unzGoToFirstFile(myZip);

    if (ret != UNZ_OK)
    {
        cerr << "unzGoToFirstFile() call failed." << endl;
        return(retorno);
    }

    while (ret == UNZ_OK){
        retorno = extraerFichero(&myZip, rutaDest, forcedFilename, uncompress);
        if (retorno.romsize > tempRetorno.romsize){
            tempRetorno.errorCode = retorno.errorCode;
            tempRetorno.romsize = retorno.romsize;
            tempRetorno.rutaEscritura = retorno.rutaEscritura;
			
			FileProps fileprop;
            fileprop.filename = dir.getFileName(retorno.rutaEscritura);
            fileprop.dir = dir.getFolder(retorno.rutaEscritura);
			fileprop.extension = dir.getExtension(fileprop.filename);
            fileprop.fileSize = retorno.romsize;
            tempRetorno.files.push_back(fileprop);
        }
        
        if (retorno.errorCode == UNZ_OK){
            tempRetorno.nFilesWritten++;
        }
        
        if (!uncompress){
            FileProps fileprop;
            fileprop.filename = dir.getFileName(retorno.rutaEscritura);
            fileprop.dir = dir.getFolder(retorno.rutaEscritura);
            fileprop.fileSize = retorno.romsize;
			fileprop.extension = dir.getExtension(fileprop.filename);
            tempRetorno.files.push_back(fileprop);
        }
        ret = unzGoToNextFile(myZip);
    }

    unzClose(myZip);
    //unzCloseCurrentFile(myZip);
    return tempRetorno;
}

/**
*
*/
unzippedFileInfo UnzipTool::extraerFichero(unzFile *myZip, const char *ruta, const char *forcedFilename, bool uncompress){
	 int ret;
    unsigned long rom_size = 0;
    char filename[MAX_FILENAME];
    char *cartridge;
    unzippedFileInfo retorno;
    retorno.errorCode = -1;
    retorno.rutaEscritura = ruta;
    retorno.romsize = 0;
    unz_file_info zip_file_info;

    ret = unzGetCurrentFileInfo(*myZip, &zip_file_info, filename, MAX_FILENAME, NULL, 0, NULL, 0);
    if (ret == UNZ_OK){
        LOG_DEBUG("Uncompressed size is in bytes %zu\n", zip_file_info.uncompressed_size);
        rom_size = zip_file_info.uncompressed_size;
        retorno.romsize = rom_size;
    }

    ret = unzOpenCurrentFile(*myZip);
    if (ret != UNZ_OK){
        LOG_ERROR("Unable to open file from zip archive.");
        return(retorno);
    }
    
    if (uncompress){
        cartridge = new char[rom_size];
        if (cartridge == NULL){
            LOG_ERROR("Unable allocate memory for cartridge.");
            return(retorno);
        }

        // Load rom file.
        ret = unzReadCurrentFile(*myZip, cartridge, rom_size);
        dirutil dir;
		
		string filenameOut = dir.getFolder(ruta) + Constant::getFileSep();
		
		//To avoid writing errors due to the name of the rom, we can force the filename
		if (forcedFilename != NULL){
			string original_extension = dir.getExtension(filename);
			filenameOut = filenameOut + forcedFilename + original_extension;
		} else {
			filenameOut = filenameOut + filename;
		}
		
		Fileio fileio;
        if (fileio.writeToFile(filenameOut.c_str(), cartridge, rom_size, false) == 1){
            retorno.rutaEscritura = filenameOut;
            retorno.errorCode = UNZ_OK;
        } else {
            LOG_ERROR("Error UnzipTool::extraerFichero");
        }
        delete [] cartridge;
    } else {
        retorno.rutaEscritura = filename;
    }

    return retorno;
}


/**
*
*/
unzippedFileInfo UnzipTool::descomprimirZipToMem(std::string rutaZip, std::string validExtensions, void*& cartridge){
    unzFile myZip = NULL;
	myZip = unzOpen(rutaZip.c_str());
    unz_global_info zip_global_info;
    int ret;
    dirutil dir;
	bool uncompress = true;
    unzippedFileInfo retorno;
    
    retorno.errorCode = -1;
    retorno.rutaEscritura = rutaZip;
    retorno.romsize = 0;
    retorno.nFilesInZip = 0;
    retorno.nFilesWritten = 0;

    if (myZip == NULL){
		LOG_ERROR("UnzipTool::descomprimirZip. Unable to open zip file: %s\n", rutaZip.c_str());
        return retorno;
    }
    else{
		LOG_DEBUG("UnzipTool::descomprimirZip. File found: %s\n", rutaZip.c_str());
    }

    ret = unzGetGlobalInfo(myZip, &zip_global_info);
    if (ret != UNZ_OK){
		LOG_ERROR("unzGetGlobalInfo() call failed.\n");
        return retorno;
    } else {
		LOG_DEBUG("Found files in archive: %d\n", zip_global_info.number_entry);
    }

    ret = unzGoToFirstFile(myZip);
    if (ret != UNZ_OK){
		LOG_ERROR("unzGoToFirstFile() call failed.\n");
        return retorno;
    }

    while (ret == UNZ_OK){
        retorno = extraerFicheroToMem(&myZip, validExtensions, zip_global_info.number_entry, cartridge);
		if (retorno.nFilesInZip == 1 || retorno.romsize > 0){
			break;
		}
		ret = unzGoToNextFile(myZip); // Avanzar al siguiente
    }
    unzClose(myZip);
    return retorno;
}

unzippedFileInfo UnzipTool::extraerFicheroToMem(unzFile *myZip, std::string validExtensions, int nfiles, void*& cartridge){
	int ret;
	char filename[MAX_FILENAME];
	unz_file_info zip_file_info;
	dirutil dir;
	unzippedFileInfo retorno;
	
	retorno.errorCode = -1;
    retorno.rutaEscritura = "";
    retorno.romsize = 0;
    retorno.nFilesInZip = 0;
    retorno.nFilesWritten = 0;

	retorno.romsize = 0;
	ret = unzGetCurrentFileInfo(*myZip, &zip_file_info, filename, MAX_FILENAME, NULL, 0, NULL, 0);

	if (ret == UNZ_OK){
		LOG_DEBUG("Uncompressed size is in bytes: %zu\n", zip_file_info.uncompressed_size);
        retorno.romsize = zip_file_info.uncompressed_size;
    }

    ret = unzOpenCurrentFile(*myZip);
    if (ret != UNZ_OK){
		LOG_ERROR("Unable to open file from zip archive.\n");
        retorno.romsize = 0;
		return retorno;
    }

	cartridge = malloc(retorno.romsize); 
    if (cartridge == NULL){
		LOG_ERROR("Unable allocate memory for cartridge.\n");
        retorno.romsize = 0;
		return retorno;
    }

    // Load rom file.
    ret = unzReadCurrentFile(*myZip, cartridge, retorno.romsize);
	// SIEMPRE cerrar el fichero interno después de leerlo
	unzCloseCurrentFile(*myZip); 

	std::string extension = Constant::replaceAll(dir.getExtension(string(filename)), ".", "");
	if (ret >= 0 && (nfiles == 1 || validExtensions.find(extension) != std::string::npos)){
		retorno.nFilesInZip = nfiles;
		retorno.rutaEscritura = filename;
		retorno.errorCode = UNZ_OK;
		return retorno;
	} else {
		free(cartridge);
		cartridge = NULL; 
		retorno.romsize = 0;
		return retorno;
	}
}