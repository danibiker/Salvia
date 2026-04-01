#include <stdio.h>
#include <stdint.h>

#include <stdlib.h>   /* calloc / free        */
#include <errno.h>    /* EEXIST               */
#include <string.h>   /* strcmp               */
#ifdef _XBOX
#  include <xtl.h>
#  include <direct.h>   /* _mkdir              */
#  include <sys/stat.h> /* _stat64             */
#  include <io.h>        /* ← _chsize_s, _fileno */
#elif defined(_WIN32)
#  include <windows.h>  /* FindFirstFile, etc. */
#  include <direct.h>   /* _mkdir              */
#  include <sys/stat.h> /* _stat64             */
#  include <io.h>        /* ← _chsize_s, _fileno */
#else
#  include <dirent.h>   /* opendir / readdir   */
#  include <sys/stat.h> /* stat / mkdir        */
#  include <unistd.h>   /* ftruncate           */
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

//Si en el futuro quieres implementar un botón de "Cambiar Disco" en tu interfaz Salvia, usarías las funciones que acabas de guardar:
//disk_control.set_eject_state(true) para abrir la bandeja.
//disk_control.replace_image_index(index, &info) para cambiar el CHD.
struct retro_disk_control_callback disk_control;
struct retro_disk_control_ext_callback disk_control_ext;

// Implementaciones mínimas usando stdio.h
const char* vfs_get_path_impl(struct retro_vfs_file_handle* stream) { return NULL; }

struct retro_vfs_file_handle* vfs_open_impl(const char* path, unsigned mode, unsigned hints) {
    if (!path || !*path) return NULL;
    
    const char* mode_str = "rb"; // Para CHD siempre lectura binaria
    if (mode & RETRO_VFS_FILE_ACCESS_WRITE) mode_str = "wb";
    if (mode & RETRO_VFS_FILE_ACCESS_UPDATE) mode_str = "rb+";

    FILE* fp = fopen(path, mode_str);
    if (!fp) {
        LOG_ERROR("Fallo al abrir archivo: %s", path);
        return NULL;
    }
    return (struct retro_vfs_file_handle*)fp;
}

int vfs_close_impl(struct retro_vfs_file_handle* stream) {
    return fclose((FILE*)stream);
}

int64_t vfs_size_impl(struct retro_vfs_file_handle* stream) {
    long pos = ftell((FILE*)stream);
    fseek((FILE*)stream, 0, SEEK_END);
    int64_t size = _ftelli64((FILE*)stream);
    fseek((FILE*)stream, pos, SEEK_SET);
    return size;
}

int64_t vfs_tell_impl(struct retro_vfs_file_handle* stream) {
    return _ftelli64((FILE*)stream);
}

// Usa int64_t explícitamente de <stdint.h>
int64_t vfs_seek_impl(struct retro_vfs_file_handle* stream, int64_t offset, int seek_position) {
    if (!stream) return -1;
    
    int whence;
    switch (seek_position) {
        case 0: whence = SEEK_SET; break; // RETRO_VFS_SEEK_POSITION_START
        case 1: whence = SEEK_CUR; break; // RETRO_VFS_SEEK_POSITION_CURRENT
        case 2: whence = SEEK_END; break; // RETRO_VFS_SEEK_POSITION_END
        default: whence = SEEK_SET;
    }
    
    // IMPORTANTE: En MSVC usa _fseeki64 para manejar el offset de 64 bits correctamente
    return _fseeki64((FILE*)stream, offset, whence);
}

int64_t vfs_read_impl(struct retro_vfs_file_handle* stream, void* s, uint64_t len) {
    return fread(s, 1, (size_t)len, (FILE*)stream);
}

int64_t vfs_write_impl(struct retro_vfs_file_handle* stream, const void* s, uint64_t len) {
    return fwrite(s, 1, (size_t)len, (FILE*)stream);
}

int vfs_flush_impl(struct retro_vfs_file_handle* stream) {
    return fflush((FILE*)stream);
}

int vfs_remove_impl(const char* path) { return remove(path); }
int vfs_rename_impl(const char* old_path, const char* new_path) { return rename(old_path, new_path); }

/* ─────────────────────────────────────────────────────────────
   VFS API v2 — truncate
   ───────────────────────────────────────────────────────────── */

int64_t vfs_truncate_impl(struct retro_vfs_file_handle* stream, int64_t length)
{
    if (!stream || length < 0) return -1;
#if defined(_WIN32)
    return _chsize_s(_fileno((FILE*)stream), length);
#else
    return ftruncate(fileno((FILE*)stream), (off_t)length);
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
    if (_stat64(path, &info) != 0) return 0;

    int flags = RETRO_VFS_STAT_IS_VALID;
    if (info.st_mode & _S_IFDIR) flags |= RETRO_VFS_STAT_IS_DIRECTORY;
    if (!(info.st_mode & _S_IFREG)) flags |= RETRO_VFS_STAT_IS_CHARACTER_SPECIAL;
    if (size) *size = (int32_t)info.st_size;
#else
    struct stat info;
    if (stat(path, &info) != 0) return 0;

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
#if defined(_WIN32)
    int ret = _mkdir(dir);
#else
    int ret = mkdir(dir, 0755);
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
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    h->find_handle = FindFirstFileA(pattern, &h->find_data);
    if (h->find_handle == INVALID_HANDLE_VALUE) { free(h); return NULL; }
    h->first = true;
#else
    h->dir = opendir(dir);
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


