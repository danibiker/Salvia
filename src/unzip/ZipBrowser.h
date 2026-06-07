#pragma once
// ZipBrowser.h
// Requiere: unzip/minizip-1.2.5/unzip.h y unzip/zlib.h
// Compilado con Visual Studio 2010

#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include "unzip/minizip-1.2.5/unzip.h"
#include <direct.h>        // _mkdir (Windows)

// -----------------------------------------------------------------------
// Estructuras de resultado
// -----------------------------------------------------------------------

struct ZipEntry
{
    std::string name;       // Nombre del fichero o carpeta (sin ruta)
    std::string fullPath;   // Ruta completa dentro del ZIP
    bool        isDir;      // true si es directorio
    uLong       uncompressedSize;
    uLong       compressedSize;
};

// -----------------------------------------------------------------------
// ZipBrowser
// Permite abrir un .zip y listar su contenido nivel a nivel.
//
// Uso:
//   ZipBrowser zb;
//   if (zb.Open("mi_archivo.zip"))
//   {
//       // Listar raiz
//       std::vector<ZipEntry> entries = zb.ListDirectory("");
//
//       // Listar subcarpeta
//       entries = zb.ListDirectory("carpeta/subcarpeta/");
//
//       zb.Close();
//   }
// -----------------------------------------------------------------------
class ZipBrowser
{
public:

    ZipBrowser() : m_hZip(NULL) {}
    ~ZipBrowser() { Close(); }

    // Abre el fichero ZIP. Devuelve true si tiene exito.
    bool Open(const std::string& zipPath)
    {
        Close();
        m_hZip = unzOpen(zipPath.c_str());
        return (m_hZip != NULL);
    }

    // Cierra el fichero ZIP abierto.
    void Close()
    {
        if (m_hZip)
        {
            unzClose(m_hZip);
            m_hZip = NULL;
        }
    }

    // Devuelve true si hay un ZIP abierto.
    bool IsOpen() const { return m_hZip != NULL; }

    // -----------------------------------------------------------------------
    // ListDirectory
    //
    // Lista el contenido de una "carpeta virtual" dentro del ZIP.
    //
    // Parametros:
    //   prefix  - ruta interna de la carpeta que quieres listar.
    //             - Para la raiz: "" o "/"
    //             - Para una subcarpeta: "carpeta/"  o  "carpeta/sub/"
    //             (se normaliza internamente, no hace falta el / final)
    //
    // Devuelve un vector con las entradas DIRECTAS bajo ese prefijo
    // (un nivel de profundidad).  Las carpetas aparecen con isDir=true.
    // -----------------------------------------------------------------------
    std::vector<ZipEntry> ListDirectory(const std::string& prefixRaw) const
    {
        std::vector<ZipEntry> result;

        if (!m_hZip)
            return result;

        // Normalizar prefijo: sin / inicial, con / final (excepto raiz)
        std::string prefix = NormalizePrefix(prefixRaw);

        // Para evitar duplicar carpetas intermedias usamos un set
        std::set<std::string> seenDirs;

        // Ir a la primera entrada
        int ret = unzGoToFirstFile(m_hZip);
        while (ret == UNZ_OK)
        {
            unz_file_info info;
            char szName[512] = {0};

            if (unzGetCurrentFileInfo(m_hZip, &info, szName, sizeof(szName),
                                      NULL, 0, NULL, 0) == UNZ_OK)
            {
                std::string entryPath(szName);

                // Normalizar separadores (algunos ZIPs usan '\')
                std::replace(entryPath.begin(), entryPath.end(), '\\', '/');

                // Comprobar que la entrada esta bajo nuestro prefijo
                if (entryPath.size() > prefix.size() &&
                    entryPath.substr(0, prefix.size()) == prefix)
                {
                    // Parte relativa a partir del prefijo
                    std::string relative = entryPath.substr(prefix.size());

                    // Buscar si hay un '/' en la parte relativa
                    size_t slashPos = relative.find('/');

                    if (slashPos == std::string::npos)
                    {
                        // ---- Es un fichero directo en este nivel ----
                        ZipEntry e;
                        e.name             = relative;
                        e.fullPath         = entryPath;
                        e.isDir            = false;
                        e.uncompressedSize = info.uncompressed_size;
                        e.compressedSize   = info.compressed_size;
                        result.push_back(e);
                    }
                    else if (slashPos == relative.size() - 1)
                    {
                        // ---- Es la entrada de directorio directo ----
                        // (la propia carpeta registrada en el ZIP)
                        std::string dirName = relative.substr(0, slashPos);
                        if (seenDirs.find(dirName) == seenDirs.end())
                        {
                            seenDirs.insert(dirName);
                            ZipEntry e;
                            e.name             = dirName;
                            e.fullPath         = entryPath;
                            e.isDir            = true;
                            e.uncompressedSize = 0;
                            e.compressedSize   = 0;
                            result.push_back(e);
                        }
                    }
                    else
                    {
                        // ---- Fichero en subcarpeta: mostrar la carpeta ----
                        std::string dirName = relative.substr(0, slashPos);
                        if (seenDirs.find(dirName) == seenDirs.end())
                        {
                            seenDirs.insert(dirName);
                            ZipEntry e;
                            e.name             = dirName;
                            e.fullPath         = prefix + dirName + "/";
                            e.isDir            = true;
                            e.uncompressedSize = 0;
                            e.compressedSize   = 0;
                            result.push_back(e);
                        }
                    }
                }
            }

            ret = unzGoToNextFile(m_hZip);
        }

        // Ordenar: carpetas primero, luego ficheros, ambos alfabeticamente
        std::sort(result.begin(), result.end(), EntryComparator);

        return result;
    }

    // -----------------------------------------------------------------------
    // GetAllEntries - devuelve TODAS las entradas del ZIP sin filtrar
    // -----------------------------------------------------------------------
    std::vector<ZipEntry> GetAllEntries() const
    {
        std::vector<ZipEntry> result;
        if (!m_hZip) return result;

        int ret = unzGoToFirstFile(m_hZip);
        while (ret == UNZ_OK)
        {
            unz_file_info info;
            char szName[512] = {0};

            if (unzGetCurrentFileInfo(m_hZip, &info, szName, sizeof(szName),
                                      NULL, 0, NULL, 0) == UNZ_OK)
            {
                std::string entryPath(szName);
                std::replace(entryPath.begin(), entryPath.end(), '\\', '/');

                bool isDir = (!entryPath.empty() &&
                              entryPath[entryPath.size()-1] == '/');

                ZipEntry e;
                e.fullPath         = entryPath;
                e.isDir            = isDir;
                e.uncompressedSize = info.uncompressed_size;
                e.compressedSize   = info.compressed_size;
                e.name             = isDir
                    ? entryPath.substr(0, entryPath.size()-1)
                    : entryPath;

                // Quedarnos solo con el nombre final
                size_t lastSlash = e.name.rfind('/');
                if (lastSlash != std::string::npos)
                    e.name = e.name.substr(lastSlash + 1);

                result.push_back(e);
            }
            ret = unzGoToNextFile(m_hZip);
        }
        return result;
    }

	// -----------------------------------------------------------------------
    // GetPathType
    //
    // Determina si una ruta interna del ZIP es un fichero, una carpeta,
    // o no existe. La ruta se puede pasar con o sin '/' final y con o
    // sin '/' inicial; se normaliza internamente.
    //
    // Devuelve uno de los valores del enum PathType:
    //   PATH_NOT_FOUND  - la ruta no existe en el ZIP
    //   PATH_FILE       - es un fichero
    //   PATH_DIR        - es una carpeta (tiene entradas bajo ella)
    //
    // Ejemplos:
    //   GetPathType("")              -> PATH_DIR   (raiz, siempre existe)
    //   GetPathType("docs/")         -> PATH_DIR
    //   GetPathType("docs/manual.pdf") -> PATH_FILE
    //   GetPathType("no/existe")     -> PATH_NOT_FOUND
    // -----------------------------------------------------------------------
    enum PathType { PATH_NOT_FOUND, PATH_FILE, PATH_DIR };
 
    // -----------------------------------------------------------------------
    // GetPathType
    //
    // Determina si una ruta interna del ZIP es un fichero, una carpeta,
    // o no existe. La ruta se puede pasar con o sin '/' final y con o
    // sin '/' inicial; se normaliza internamente.
    //
    // Devuelve uno de los valores del enum PathType:
    //   PATH_NOT_FOUND  - la ruta no existe en el ZIP
    //   PATH_FILE       - es un fichero
    //   PATH_DIR        - es una carpeta (tiene entradas bajo ella)
    //
    // Ejemplos:
    //   GetPathType("")              -> PATH_DIR   (raiz, siempre existe)
    //   GetPathType("docs/")         -> PATH_DIR
    //   GetPathType("docs/manual.pdf") -> PATH_FILE
    //   GetPathType("no/existe")     -> PATH_NOT_FOUND
    // -----------------------------------------------------------------------
 
    PathType GetPathType(const std::string& pathRaw) const
    {
        if (!m_hZip)
            return PATH_NOT_FOUND;
 
        // La raiz siempre es un directorio valido
        std::string normalized = pathRaw;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
 
        // Quitar '/' inicial
        if (!normalized.empty() && normalized[0] == '/')
            normalized = normalized.substr(1);
 
        if (normalized.empty())
            return PATH_DIR;
 
        // Quitar '/' final para trabajar con la forma canonica sin slash
        std::string pathNoSlash = normalized;
        if (pathNoSlash[pathNoSlash.size()-1] == '/')
            pathNoSlash = pathNoSlash.substr(0, pathNoSlash.size()-1);
 
        // La forma con slash al final (para comparar entradas de directorio)
        std::string pathWithSlash = pathNoSlash + '/';
 
        int ret = unzGoToFirstFile(m_hZip);
        while (ret == UNZ_OK)
        {
            unz_file_info info;  // minizip 1.2.5 no acepta NULL aqui: escribe sin comprobar
            char szName[512] = {0};
            if (unzGetCurrentFileInfo(m_hZip, &info, szName, sizeof(szName),
                                      NULL, 0, NULL, 0) == UNZ_OK)
            {
                std::string entry(szName);
                std::replace(entry.begin(), entry.end(), '\\', '/');
 
                // Coincidencia exacta como fichero
                if (entry == pathNoSlash)
                    return PATH_FILE;
 
                // Coincidencia exacta como entrada de directorio registrada
                if (entry == pathWithSlash)
                    return PATH_DIR;
 
                // Aunque la carpeta no este registrada como entrada propia,
                // puede existir implicitamente si hay ficheros bajo ella
                if (entry.size() > pathWithSlash.size() &&
                    entry.substr(0, pathWithSlash.size()) == pathWithSlash)
                    return PATH_DIR;
            }
            ret = unzGoToNextFile(m_hZip);
        }
 
        return PATH_NOT_FOUND;
    }

	// -----------------------------------------------------------------------
    // ExtractFile
    //
    // Extrae un fichero concreto del ZIP a un destino en disco.
    //
    // Parametros:
    //   internalPath - ruta interna del fichero dentro del ZIP.
    //                  Acepta '/' o '\' como separador, con o sin '/' inicial.
    //                  Ej: "docs/manual.pdf"  o  "1 Japan\rom.bin"
    //   dest         - destino en disco. Dos modos segun como termine la ruta:
    //
    //                  MODO DIRECTORIO  (termina en '/' o '\')
    //                    El fichero se extrae dentro de esa carpeta conservando
    //                    el nombre original del ZIP.
    //                    Ej: "C:/temp/"  ->  C:/temp/rom.bin
    //
    //                  MODO FICHERO  (no termina en '/' ni '\')
    //                    El fichero se escribe exactamente con la ruta y nombre
    //                    indicados, ignorando el nombre original del ZIP.
    //                    Ej: "C:/temp/mirom.bin"  ->  C:/temp/mirom.bin
    //
    //                  En ambos modos se crean automaticamente los directorios
    //                  intermedios que no existan.
    //
    // Devuelve:
    //   true  - extraccion correcta
    //   false - ruta interna no encontrada, apunta a directorio, o error de E/S
    //
    // Ejemplos:
    //   zb.ExtractFile("1 Japan/rom.bin", "C:/temp/");
    //   // Genera: C:/temp/rom.bin          (modo directorio)
    //
    //   zb.ExtractFile("1 Japan/rom.bin", "C:/temp/mirom.bin");
    //   // Genera: C:/temp/mirom.bin        (modo fichero, nombre propio)
    // -----------------------------------------------------------------------
    bool ExtractFile(const std::string& internalPath,
                     const std::string& dest) const
    {
        if (!m_hZip)
            return false;
 
        // --- Normalizar ruta interna ---
        // Separadores a '/', sin '/' al inicio ni al final
        std::string normalized = internalPath;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        if (!normalized.empty() && normalized[0] == '/')
            normalized = normalized.substr(1);
        if (!normalized.empty() && normalized[normalized.size()-1] == '/')
            normalized = normalized.substr(0, normalized.size()-1);
 
        if (normalized.empty())
            return false;
 
        // --- Determinar ruta de salida antes de buscar en el ZIP ---
        // Si 'dest' termina en '/' o '\' => modo directorio
        // En caso contrario            => modo fichero (nombre ya incluido)
        std::string destNorm = dest;
        std::replace(destNorm.begin(), destNorm.end(), '\\', '/');
 
        std::string destPath;
        bool destIsDir = (!destNorm.empty() && destNorm[destNorm.size()-1] == '/');
 
        if (destIsDir)
        {
            // Modo directorio: usar el nombre final del fichero interno
            std::string fileName = normalized;
            std::size_t lastSlash = fileName.rfind('/');
            if (lastSlash != std::string::npos)
                fileName = fileName.substr(lastSlash + 1);
 
            CreateDirRecursive(destNorm);
            destPath = destNorm + fileName;
        }
        else
        {
            // Modo fichero: la ruta de salida es exactamente 'dest'
            // Crear solo la carpeta que lo contiene
            std::string parentDir = destNorm;
            std::size_t lastSlash = parentDir.rfind('/');
            if (lastSlash != std::string::npos)
                CreateDirRecursive(parentDir.substr(0, lastSlash + 1));
 
            destPath = destNorm;
        }
 
        // --- Buscar la entrada recorriendo el ZIP ---
        bool found = false;
        int ret = unzGoToFirstFile(m_hZip);
        while (ret == UNZ_OK && !found)
        {
            unz_file_info info;
            char szName[512] = {0};
 
            if (unzGetCurrentFileInfo(m_hZip, &info, szName, sizeof(szName),
                                      NULL, 0, NULL, 0) != UNZ_OK)
            {
                ret = unzGoToNextFile(m_hZip);
                continue;
            }
 
            std::string entry(szName);
            std::replace(entry.begin(), entry.end(), '\\', '/');
 
            // Si la entrada termina en '/' es un directorio: no extraer
            if (!entry.empty() && entry[entry.size()-1] == '/')
            {
                ret = unzGoToNextFile(m_hZip);
                continue;
            }
 
            if (entry != normalized)
            {
                ret = unzGoToNextFile(m_hZip);
                continue;
            }
 
            // --- Entrada encontrada ---
            found = true;
 
            // Abrir la entrada para descompresion
            if (unzOpenCurrentFile(m_hZip) != UNZ_OK)
                return false;
			
#ifdef _XBOX
			std::replace(destPath.begin(), destPath.end(), '/', '\\');
#endif
            // Abrir fichero de salida en modo binario
            FILE* fOut = fopen(destPath.c_str(), "wb");
            if (!fOut)
            {
                unzCloseCurrentFile(m_hZip);
                return false;
            }
 
            // Leer en bloques y escribir
            static const int BUFFER_SIZE = 65536;  // 64 KB
            char buffer[BUFFER_SIZE];
            int  bytesRead = 0;
            bool writeOk   = true;
 
            while ((bytesRead = unzReadCurrentFile(m_hZip, buffer, BUFFER_SIZE)) > 0)
            {
                if (fwrite(buffer, 1, (std::size_t)bytesRead, fOut) != (std::size_t)bytesRead)
                {
                    writeOk = false;
                    break;
                }
            }
 
            fclose(fOut);
            unzCloseCurrentFile(m_hZip);
 
            // bytesRead < 0 => error de descompresion (CRC, datos corruptos, etc.)
            if (bytesRead < 0 || !writeOk)
                return false;
 
            return true;
        }
 
        return false;  // No encontrado
    }
private:

    unzFile m_hZip;

	// -----------------------------------------------------------------------
    // CreateDirRecursive
    // Crea todos los directorios de 'path' que no existan todavia.
    // 'path' debe usar '/' como separador y terminar en '/'.
    // Los directorios ya existentes se ignoran silenciosamente.
    // -----------------------------------------------------------------------
    static void CreateDirRecursive(const std::string& path)
    {
        std::string current;
        current.reserve(path.size());
 
        for (std::size_t i = 0; i < path.size(); ++i)
        {
            current += path[i];
            if (path[i] == '/')
            {
                // _mkdir devuelve -1 si ya existe: lo ignoramos
                _mkdir(current.c_str());
            }
        }
    }

    // Normaliza el prefijo: sin '/' inicial, con '/' final (salvo "")
    static std::string NormalizePrefix(const std::string& raw)
    {
        std::string p = raw;
        std::replace(p.begin(), p.end(), '\\', '/');

        // Quitar '/' al inicio
        if (!p.empty() && p[0] == '/')
            p = p.substr(1);

        // Añadir '/' al final si no lo tiene (y no es vacio)
        if (!p.empty() && p[p.size()-1] != '/')
            p += '/';

        return p;
    }

    // Comparador: carpetas antes que ficheros, luego alfabetico
    static bool EntryComparator(const ZipEntry& a, const ZipEntry& b)
    {
        if (a.isDir != b.isDir)
            return a.isDir > b.isDir; // directorios primero
        return a.name < b.name;
    }
};