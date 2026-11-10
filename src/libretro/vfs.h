#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>   /* calloc / free        */
#include <errno.h>    /* EEXIST               */
#include <string.h>   /* strcmp               */

#include <utils\logger.h>

#ifdef _XBOX
#  include <xtl.h>
#  include <direct.h>   /* _mkdir              */
#  include <sys/stat.h> /* _stat64             */
#  include <io.h>        /* ← _chsize_s, _fileno */
#elif defined(_WIN32)
#  include <windows.h>  /* FindFirstFile, etc. */
#  include <direct.h>   /* _mkdir              */
#  include <sys/stat.h> /* _stat64             */
#  include <io.h>       /* _chsize_s, _fileno */
#else
#  include <dirent.h>   /* opendir / readdir   */
#  include <sys/stat.h> /* stat / mkdir        */
#  include <unistd.h>   /* ftruncate           */
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif

#ifndef RETRO_VFS_FILE_ACCESS_READ
#define RETRO_VFS_FILE_ACCESS_READ         (1 << 0)
#endif
#ifndef RETRO_VFS_FILE_ACCESS_WRITE
#define RETRO_VFS_FILE_ACCESS_WRITE        (1 << 1)
#endif
#ifndef RETRO_VFS_FILE_ACCESS_UPDATE
#define RETRO_VFS_FILE_ACCESS_UPDATE       (1 << 2)
#endif

// Implementaciones mínimas usando stdio.h
const char* vfs_get_path_impl(struct retro_vfs_file_handle* stream) { return NULL; }

// Función que normaliza separadores y limpia múltiples barras seguidas
std::string limpiarRutaWindows(std::string ruta) {
    
    // 1. Reemplazar todas las barras unix '/' por barras windows '\'
    std::replace(ruta.begin(), ruta.end(), '/', '\\');

    // 2. Eliminar barras invertidas consecutivas duplicadas
    std::string::iterator nuevo_final = std::unique(ruta.begin(), ruta.end(), 
        [](char a, char b) {
            return a == '\\' && b == '\\';
        }
    );

    // 3. Recortar el string para eliminar los caracteres sobrantes del final
    ruta.erase(nuevo_final, ruta.end());

    return ruta;
}

//definimos pares de valores con el caracter que hay que reemplazar y la palabra con la que lo reemplazamos
const char *FORBIDDEN_REPLACE[1][2] = {{"+", "plus"}};

//Esta funcion reemplaza caracteres por otras palabras definidas
std::string replaceForbiddenChars(std::string cleanPath) {
//PATH_REPLACE es una opcion de PREPROCESADOR para no depender de codigo estatico
#ifdef PATH_REPLACE
	std::string lowPath = cleanPath;
    std::transform(lowPath.begin(), lowPath.end(), lowPath.begin(), ::tolower);
	
    if (lowPath.find(PATH_REPLACE) != std::string::npos) {
        size_t targetLen = std::strlen(FORBIDDEN_REPLACE[0][0]);      // Longitud de "+"
        size_t replaceLen = std::strlen(FORBIDDEN_REPLACE[0][1]);     // Longitud de "plus"

        size_t pos = cleanPath.find(FORBIDDEN_REPLACE[0][0]);
        while (pos != std::string::npos) {
            std::string oldPath = cleanPath; 
            cleanPath.replace(pos, targetLen, FORBIDDEN_REPLACE[0][1]);
            LOG_DEBUG("vfs_open: Modified path %s to %s", oldPath.c_str(), cleanPath.c_str());
            // Avanza de forma segura sumando la longitud real del reemplazo
            pos = cleanPath.find(FORBIDDEN_REPLACE[0][0], pos + replaceLen);
        }
    }
#endif
    return cleanPath;
}

/* ─────────────────────────────────────────────────────────────
   Ventana de lectura (solo handles de solo-lectura)

   El stdio de Xbox 360 descarta su buffer interno con cada
   _fseeki64, de modo que un core que lee con seek+fread pequeño
   (puerto de datos MSU-1, cabeceras, tablas...) convierte cada
   petición en E/S física al dispositivo. Para handles de lectura
   mantenemos una ventana en RAM: las peticiones dentro de la
   ventana se sirven con memcpy y solo el relleno (bloques de
   64 KB) toca el disco. Los handles de escritura/update quedan
   sin buffer, así no hay coherencia que mantener.
   ───────────────────────────────────────────────────────────── */
#define VFS_READ_BUF_CAP 65536

typedef struct vfs_file {
    FILE*    fp;
    uint8_t* buf;        /* NULL en handles de escritura/update */
    uint64_t buf_base;   /* offset de fichero de buf[0]         */
    uint32_t buf_len;    /* bytes válidos en buf                */
    uint64_t pos;        /* posición lógica                     */
} vfs_file_t;

/* Sitúa el FILE en la posición lógica y rellena la ventana. */
static void vfs_buf_fill(vfs_file_t* f)
{
    if (_fseeki64(f->fp, (int64_t)f->pos, SEEK_SET) != 0) { f->buf_len = 0; return; }
    f->buf_base = f->pos;
    f->buf_len  = (uint32_t)fread(f->buf, 1, VFS_READ_BUF_CAP, f->fp);
}

struct retro_vfs_file_handle* vfs_open_impl(const char* path, unsigned mode, unsigned hints) {
    if (!path || !*path) return NULL;
    
    const char* mode_str = "rb"; // Para CHD siempre lectura binaria
    if (mode & RETRO_VFS_FILE_ACCESS_WRITE) mode_str = "wb";
    if (mode & RETRO_VFS_FILE_ACCESS_UPDATE) mode_str = "rb+";

	std::string cleanPath = replaceForbiddenChars(limpiarRutaWindows(path));
	
	FILE* fp = fopen(cleanPath.c_str(), mode_str);
    if (!fp) {
        LOG_ERROR("Fallo al abrir archivo: %s", cleanPath.c_str());
        return NULL;
    }

    vfs_file_t* f = (vfs_file_t*)calloc(1, sizeof(vfs_file_t));
    if (!f) { fclose(fp); return NULL; }
    f->fp = fp;
    /* Solo el camino de lectura se bufferiza: es el caliente (ROMs,
       MSU-1, ISOs...) y el único sin escrituras que mantener. */
    if (!(mode & (RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE)))
        f->buf = (uint8_t*)malloc(VFS_READ_BUF_CAP);
    return (struct retro_vfs_file_handle*)f;
}

int vfs_close_impl(struct retro_vfs_file_handle* stream) {
    vfs_file_t* f = (vfs_file_t*)stream;
    if (!f) return -1;
    int ret = fclose(f->fp);
    if (f->buf) free(f->buf);
    free(f);
    return ret;
}

int64_t vfs_size_impl(struct retro_vfs_file_handle* stream) {
    vfs_file_t* f = (vfs_file_t*)stream;
    if (!f) return -1;
    if (f->buf) {
        /* Medir mueve el FILE, pero no importa: la ventana queda
           validada por su base/longitud y el relleno re-sitúa el
           FILE a la posición lógica antes de leer. */
        if (_fseeki64(f->fp, 0, SEEK_END) != 0) return -1;
        return _ftelli64(f->fp);
    }
    long pos = ftell(f->fp);
    _fseeki64(f->fp, 0, SEEK_END);
    int64_t size = _ftelli64(f->fp);
    _fseeki64(f->fp, pos, SEEK_SET);
    return size;
}

int64_t vfs_tell_impl(struct retro_vfs_file_handle* stream) {
    vfs_file_t* f = (vfs_file_t*)stream;
    if (!f) return -1;
    if (f->buf) return (int64_t)f->pos;
    return _ftelli64(f->fp);
}

int64_t vfs_seek_impl(struct retro_vfs_file_handle* stream, int64_t offset, int seek_position) {
    vfs_file_t* f = (vfs_file_t*)stream;
    if (!f) return -1;

    int whence;
    switch (seek_position) {
        case 0: whence = SEEK_SET; break; // RETRO_VFS_SEEK_POSITION_START
        case 1: whence = SEEK_CUR; break; // RETRO_VFS_SEEK_POSITION_CURRENT
        case 2: whence = SEEK_END; break; // RETRO_VFS_SEEK_POSITION_END
        default: whence = SEEK_SET;
    }

    if (f->buf) {
        int64_t target;
        switch (seek_position) {
            case 0: target = offset; break;
            case 1: target = (int64_t)f->pos + offset; break;
            case 2:
                if (_fseeki64(f->fp, 0, SEEK_END) != 0) return -1;
                target = _ftelli64(f->fp) + offset;
                break;
            default: target = offset;
        }
        if (target < 0) return -1;
        f->pos = (uint64_t)target;
        /* Seek dentro de la ventana: gratis, sin tocar el FILE.
           Fuera de ella: se descarta y el próximo read rellena. */
        if (target < (int64_t)f->buf_base ||
            target >= (int64_t)(f->buf_base + f->buf_len))
            f->buf_len = 0;
        return target;
    }

    // IMPORTANTE: En MSVC usa _fseeki64 para manejar el offset de 64 bits correctamente
    return _fseeki64(f->fp, offset, whence);
}

int64_t vfs_read_impl(struct retro_vfs_file_handle* stream, void* s, uint64_t len) {
    vfs_file_t* f = (vfs_file_t*)stream;
    if (!f) return 0;

    if (f->buf) {
        uint8_t* out = (uint8_t*)s;
        uint64_t total = 0;
        /* Lecturas grandes: directo al FILE, la ventana no aporta. */
        if (len >= VFS_READ_BUF_CAP) {
            int64_t n;
            if (_fseeki64(f->fp, (int64_t)f->pos, SEEK_SET) != 0) return 0;
            n = (int64_t)fread(s, 1, (size_t)len, f->fp);
            if (n > 0) f->pos += (uint64_t)n;
            return n;
        }
        while (total < len) {
            uint64_t win_end = f->buf_base + f->buf_len;
            uint64_t chunk;
            if (f->pos < f->buf_base || f->pos >= win_end) {
                vfs_buf_fill(f);
                win_end = f->buf_base + f->buf_len;
                if (f->pos < f->buf_base || f->pos >= win_end)
                    break; /* EOF o fallo de E/S */
            }
            chunk = win_end - f->pos;
            if (chunk > len - total) chunk = len - total;
            memcpy(out + total, f->buf + (size_t)(f->pos - f->buf_base), (size_t)chunk);
            f->pos += chunk;
            total += chunk;
        }
        return (int64_t)total;
    }

    return fread(s, 1, (size_t)len, f->fp);
}

int64_t vfs_write_impl(struct retro_vfs_file_handle* stream, const void* s, uint64_t len) {
    vfs_file_t* f = (vfs_file_t*)stream;
    if (!f) return -1;
    return fwrite(s, 1, (size_t)len, f->fp);
}

int vfs_flush_impl(struct retro_vfs_file_handle* stream) {
    vfs_file_t* f = (vfs_file_t*)stream;
    if (!f) return -1;
    return fflush(f->fp);
}

int vfs_remove_impl(const char* path) { return remove(path); }
int vfs_rename_impl(const char* old_path, const char* new_path) { return rename(old_path, new_path); }

/* ─────────────────────────────────────────────────────────────
   VFS API v2 — truncate
   ───────────────────────────────────────────────────────────── */

int64_t vfs_truncate_impl(struct retro_vfs_file_handle* stream, int64_t length)
{
    vfs_file_t* f = (vfs_file_t*)stream;
    if (!f || length < 0) return -1;
#if defined(_WIN32)
    return _chsize_s(_fileno(f->fp), length);
#else
    return ftruncate(fileno(f->fp), (off_t)length);
#endif
}

/* ─────────────────────────────────────────────────────────────
   VFS API v3 — stat / mkdir
   ───────────────────────────────────────────────────────────── */

int vfs_stat_impl(const char* path, int32_t* size)
{
    if (!path || !*path) return 0;

#if defined(_WIN32)
    struct __stat64 info;
    if (_stat64(limpiarRutaWindows(path).c_str(), &info) != 0) return 0;

    int flags = RETRO_VFS_STAT_IS_VALID;
    if (info.st_mode & _S_IFDIR) flags |= RETRO_VFS_STAT_IS_DIRECTORY;
    if (!(info.st_mode & _S_IFREG)) flags |= RETRO_VFS_STAT_IS_CHARACTER_SPECIAL;
    if (size) *size = (int32_t)info.st_size;
#else
    struct stat info;
    if (stat(limpiarRutaWindows(path).c_str(), &info) != 0) return 0;

    int flags = RETRO_VFS_STAT_IS_VALID;
    if (S_ISDIR(info.st_mode))  flags |= RETRO_VFS_STAT_IS_DIRECTORY;
    if (!S_ISREG(info.st_mode)) flags |= RETRO_VFS_STAT_IS_CHARACTER_SPECIAL;
    if (size) *size = (int32_t)info.st_size;
#endif
    return flags;
}

int vfs_mkdir_impl(const char* dir)
{
    if (!dir || !*dir) return -1;

	std::string cleanDir = limpiarRutaWindows(dir);


#if defined(_WIN32)
    int ret = _mkdir(cleanDir.c_str());
#else
    int ret = mkdir(cleanDir.c_str(), 0755);
#endif
    /* 0 = creado, -2 = ya existía (EEXIST) */
    if (ret == 0)      return 0;
    if (errno == EEXIST) return -2;
    return -1;
}

/* ─────────────────────────────────────────────────────────────
   VFS API v3 — directorio
   Handle interno: guarda el estado del iterador de forma
   independiente en Windows (FindFirstFile) y POSIX (opendir).
   ───────────────────────────────────────────────────────────── */

struct vfs_dir_handle
{
#if defined(_WIN32)
    HANDLE           find_handle;
    WIN32_FIND_DATAA find_data;
    bool             first;          /* ¿aún no hemos avanzado? */
#else
    DIR*             dir;
    struct dirent*   entry;
#endif
    bool             include_hidden;
};

struct retro_vfs_dir_handle* vfs_opendir_impl(const char* dir, bool include_hidden)
{
    if (!dir || !*dir) return NULL;

    struct vfs_dir_handle* h =
        (struct vfs_dir_handle*)calloc(1, sizeof(struct vfs_dir_handle));
    if (!h) return NULL;

    h->include_hidden = include_hidden;

#if defined(_WIN32)
    /* Windows necesita un patrón "dir\*" */
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", limpiarRutaWindows(dir).c_str());
    h->find_handle = FindFirstFileA(pattern, &h->find_data);
    if (h->find_handle == INVALID_HANDLE_VALUE) { free(h); return NULL; }
    h->first = true;
#else
    h->dir = opendir(limpiarRutaWindows(dir).c_str());
    if (!h->dir) { free(h); return NULL; }
#endif

    return (struct retro_vfs_dir_handle*)h;
}

bool vfs_readdir_impl(struct retro_vfs_dir_handle* dirstream)
{
    struct vfs_dir_handle* h = (struct vfs_dir_handle*)dirstream;
    if (!h) return false;

#if defined(_WIN32)
    for (;;) {
        if (h->first) {
            h->first = false;          /* primer resultado ya en find_data */
        } else {
            if (!FindNextFileA(h->find_handle, &h->find_data)) return false;
        }
        /* Saltar "." y ".." siempre */
        if (strcmp(h->find_data.cFileName, ".") == 0 ||
            strcmp(h->find_data.cFileName, "..") == 0)
            continue;
        /* Saltar ocultos si no se pidieron */
        if (!h->include_hidden &&
            (h->find_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
            continue;
        return true;
    }
#else
    for (;;) {
        h->entry = readdir(h->dir);
        if (!h->entry) return false;
        if (strcmp(h->entry->d_name, ".") == 0 ||
            strcmp(h->entry->d_name, "..") == 0)
            continue;
        /* En POSIX los ocultos empiezan por '.' */
        if (!h->include_hidden && h->entry->d_name[0] == '.')
            continue;
        return true;
    }
#endif
}

const char* vfs_dirent_get_name_impl(struct retro_vfs_dir_handle* dirstream)
{
    struct vfs_dir_handle* h = (struct vfs_dir_handle*)dirstream;
    if (!h) return NULL;
#if defined(_WIN32)
    return h->find_data.cFileName;
#else
    return h->entry ? h->entry->d_name : NULL;
#endif
}

bool vfs_dirent_is_dir_impl(struct retro_vfs_dir_handle* dirstream)
{
    struct vfs_dir_handle* h = (struct vfs_dir_handle*)dirstream;
    if (!h) return false;
#if defined(_WIN32)
    return !!(h->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    if (!h->entry) return false;
    /* d_type puede no estar disponible en todos los FS; fallback con stat */
    if (h->entry->d_type != DT_UNKNOWN)
        return h->entry->d_type == DT_DIR;
    struct stat st;
    return (stat(h->entry->d_name, &st) == 0 && S_ISDIR(st.st_mode));
#endif
}

int vfs_closedir_impl(struct retro_vfs_dir_handle* dirstream)
{
    struct vfs_dir_handle* h = (struct vfs_dir_handle*)dirstream;
    if (!h) return -1;
#if defined(_WIN32)
    if (h->find_handle != INVALID_HANDLE_VALUE)
        FindClose(h->find_handle);
#else
    if (h->dir) closedir(h->dir);
#endif
    free(h);
    return 0;
}

static struct retro_vfs_interface vfs_interface = {
    /* v1 */
    vfs_get_path_impl,        /*  1 get_path      */
    vfs_open_impl,            /*  2 open          */
    vfs_close_impl,           /*  3 close         */
    vfs_size_impl,            /*  4 size          */
    vfs_tell_impl,            /*  5 tell          */
    vfs_seek_impl,            /*  6 seek          */
    vfs_read_impl,            /*  7 read          */
    vfs_write_impl,           /*  8 write         */
    vfs_flush_impl,           /*  9 flush         */
    vfs_remove_impl,          /* 10 remove        */
    vfs_rename_impl,          /* 11 rename        */
    /* v2 */
    vfs_truncate_impl,        /* 12 truncate      */
    /* v3 */
    vfs_stat_impl,            /* 13 stat          */
    vfs_mkdir_impl,           /* 14 mkdir         */
    vfs_opendir_impl,         /* 15 opendir       */
    vfs_readdir_impl,         /* 16 readdir       */
    vfs_dirent_get_name_impl, /* 17 dirent_get_name */
    vfs_dirent_is_dir_impl,   /* 18 dirent_is_dir */
    vfs_closedir_impl,        /* 19 closedir      */
};


