#include <stdio.h>
#include <stdint.h>

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

static struct retro_vfs_interface vfs_interface = {
    vfs_get_path_impl,  // 1
    vfs_open_impl,      // 2
    vfs_close_impl,     // 3
    vfs_size_impl,      // 4
    vfs_tell_impl,      // 5
    vfs_seek_impl,      // 6  <-- Aquí es donde entra core_fseek
    vfs_read_impl,      // 7
    vfs_write_impl,     // 8
    vfs_flush_impl,     // 9
    vfs_remove_impl,    // 10
    vfs_rename_impl,    // 11
    NULL,               // 12 (truncate)
    NULL,               // 13 (stat)
    NULL, NULL, NULL, NULL, NULL, NULL // Resto de punteros a NULL
};



