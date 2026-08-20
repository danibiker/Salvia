/***************************************************************************
    Binary File Loader.

    Handles loading an individual binary file to memory.
    Supports reading bytes, words and longs from this area of memory.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <encodings/crc32.h>
#include <libretro.h>
#include <retro_dirent.h>
#include <streams/file_stream.h>

#include "romloader.h"
#include <string.h>

static int RomLoader_create_map(RomLoader* self);

extern retro_log_printf_t log_cb;
extern char rom_path[1024];

typedef struct { uint32_t crc; char file[1024]; } crc_entry;
static crc_entry* crc_map     = NULL;
static int        crc_map_num = 0;
static int        crc_map_cap = 0;
static bool       map_created = false;
static char       mapped_path[1024];

static void crc_map_clear(void)
{
    free(crc_map);
    crc_map     = NULL;
    crc_map_num = 0;
    crc_map_cap = 0;
}

static void crc_map_add(uint32_t crc, const char* file)
{
    if (crc_map_num >= crc_map_cap)
    {
        int ncap        = crc_map_cap ? crc_map_cap * 2 : 64;
        crc_entry* grown = (crc_entry*)realloc(crc_map, (size_t)ncap * sizeof(crc_entry));
        if (!grown)
            return;
        crc_map     = grown;
        crc_map_cap = ncap;
    }
    crc_map[crc_map_num].crc = crc;
    strncpy(crc_map[crc_map_num].file, file, sizeof(crc_map[0].file) - 1);
    crc_map[crc_map_num].file[sizeof(crc_map[0].file) - 1] = 0;
    crc_map_num++;
}

static const char* crc_map_find(uint32_t crc)
{
    int i;
    for (i = 0; i < crc_map_num; i++)
        if (crc_map[i].crc == crc)
            return crc_map[i].file;
    return NULL;
}

static void join_path(char* out, size_t n,
                      const char* directory, const char* filename)
{
    size_t len;
    if (!directory || !directory[0])
    {
        strncpy(out, filename, n - 1);
        out[n - 1] = 0;
        return;
    }
    len = strlen(directory);
    if (directory[len - 1] == '/' || directory[len - 1] == '\\')
        snprintf(out, n, "%s%s", directory, filename);
    else
        snprintf(out, n, "%s/%s", directory, filename);
}

static bool read_exact(RFILE* file, void* data, size_t size)
{
    size_t total = 0;
    uint8_t* output = (uint8_t*)(data);

    while (total < size)
    {
        const int64_t count = filestream_read(
            file,
            output + total,
            (int64_t)(size - total));

        if (count <= 0)
            return false;

        total += (size_t)(count);
    }

    return true;
}

static bool calculate_crc32(const char* path, uint32_t* checksum)
{
    uint8_t buffer[64 * 1024];
    uint32_t crc;
    RFILE* file = filestream_open(
        path,
        RETRO_VFS_FILE_ACCESS_READ,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);

    if (!file)
        return false;

    crc = 0;

    for (;;)
    {
        const int64_t count = filestream_read(
            file,
            buffer,
            (int64_t)(sizeof(buffer)));

        if (count < 0)
        {
            filestream_close(file);
            return false;
        }

        if (count == 0)
            break;

        crc = encoding_crc32(crc, buffer, (size_t)(count));
    }

    filestream_close(file);
    *checksum = crc;
    return true;
}



void RomLoader_dtor(RomLoader* self)
{
    RomLoader_unload(self);
}

void RomLoader_init(RomLoader* self, const uint32_t new_length)
{
    RomLoader_unload(self);

    self->length = new_length;
    self->rom = (uint8_t*)malloc((self->length) * sizeof(uint8_t));
    self->loaded = false;
}

void RomLoader_unload(RomLoader* self)
{
    free(self->rom);
    self->rom = NULL;
    self->length = 0;
    self->loaded = false;
}

int RomLoader_load_auto(RomLoader* self, const char* filename,
                         const int offset,
                         const int file_length,
                         const uint32_t expected_crc,
                         const uint8_t interleave,
                         const bool verbose)
{
    /* Prefer content identification, matching the modern upstream loader. */
    /* The canonical filename remains a compatibility fallback for frontends */
    /* where directory enumeration is unavailable or for legacy ROM sets. */
    if (RomLoader_load_crc32(self, filename, offset, file_length, expected_crc,
                   interleave, false) == 0)
    {
        return 0;
    }

    if (RomLoader_load_rom(self, filename, offset, file_length, expected_crc,
                 interleave, verbose) == 0)
    {
        if (verbose && log_cb)
            log_cb(RETRO_LOG_WARN,
                   "Loaded ROM by canonical filename fallback: %s\n",
                   filename);

        return 0;
    }

    if (verbose && log_cb)
        log_cb(RETRO_LOG_ERROR,
               "Unable to locate ROM by CRC32 or canonical name: %s "
               "(CRC32: 0x%08x)\n",
               filename,
               (unsigned)(expected_crc));

    self->loaded = false;
    return 1;
}

int RomLoader_load_rom(RomLoader* self, const char* filename,
                        const int offset,
                        const int file_length,
                        const uint32_t expected_crc,
                        const uint8_t interleave,
                        const bool verbose)
{
    uint32_t checksum;
    bool read_ok;
    uint8_t* buffer;
    RFILE* source;
    char path[1024];
    join_path(path, sizeof(path), rom_path, filename);

    source = filestream_open(
        path,
        RETRO_VFS_FILE_ACCESS_READ,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);

    if (!source)
    {
        self->loaded = false;
        return 1;
    }

    buffer = (uint8_t*)malloc((size_t)file_length);
    read_ok = read_exact(source, buffer, (size_t)file_length);
    filestream_close(source);

    if (!read_ok)
    {
        if (verbose && log_cb)
            log_cb(RETRO_LOG_ERROR,
                   "ROM has an unexpected size: %s\n",
                   path);

        self->loaded = false;
        return 1;
    }

    checksum = encoding_crc32(0, buffer, (size_t)file_length);

    /* Match the existing Libretro core: report checksum mismatches, but keep */
    /* loading the named ROM instead of rejecting a set that previously worked. */
    if (expected_crc != checksum && verbose && log_cb)
    {
        log_cb(RETRO_LOG_WARN,
               "%s has incorrect checksum. Expected: 0x%08x, Found: 0x%08x\n",
               filename,
               (unsigned)(expected_crc),
               (unsigned)(checksum));
    }

    { int i; for (i = 0; i < file_length; i++)
        self->rom[(i * interleave) + offset] = buffer[i]; }

    free(buffer);

    self->loaded = true;
    return 0;
}static 

int RomLoader_create_map(RomLoader* self)
{
    RDIR* directory;
    const char* path = rom_path;

    crc_map_clear();
    strncpy(mapped_path, path, sizeof(mapped_path) - 1);
    mapped_path[sizeof(mapped_path) - 1] = 0;

    /* Mark the map as initialized even if directory enumeration fails, so a */
    /* frontend without directory VFS support does not trigger a full rescan */
    /* for every ROM before falling back to canonical filenames. */
    map_created = true;

    directory = retro_opendir(path);
    if (!directory)
    {
        if (log_cb)
            log_cb(RETRO_LOG_WARN,
                   "Could not open ROM directory for CRC32 fallback: %s\n",
                   path);

        return 1;
    }

    while (retro_readdir(directory))
    {
        uint32_t checksum;
        char     file[1024];
        const char* name = retro_dirent_get_name(directory);

        if (!name || !*name ||
            (name[0] == '.' &&
             (name[1] == '\0' ||
              (name[1] == '.' && name[2] == '\0'))) ||
            retro_dirent_is_dir(directory, NULL))
        {
            continue;
        }

        checksum = 0;
        join_path(file, sizeof(file), path, name);

        if (calculate_crc32(file, &checksum))
            crc_map_add(checksum, file);
    }

    retro_closedir(directory);

    if (crc_map_num == 0)
        return 1;

    return 0;
}

int RomLoader_load_crc32(RomLoader* self, const char* debug,
                          const int offset,
                          const int file_length,
                          const uint32_t expected_crc,
                          const uint8_t interleave,
                          const bool verbose)
{
    bool read_ok;
    uint8_t* buffer;
    RFILE* source;
    const char* match_file;
    if ((!map_created || strcmp(mapped_path, rom_path) != 0) && RomLoader_create_map(self) != 0)
    {
        self->loaded = false;
        return 1;
    }

    match_file = crc_map_find(expected_crc);

    if (!match_file)
    {
        if (verbose && log_cb)
            log_cb(RETRO_LOG_ERROR,
                   "Unable to locate ROM. Expected name: %s, CRC32: 0x%08x\n",
                   debug,
                   (unsigned)(expected_crc));

        self->loaded = false;
        return 1;
    }

    source = filestream_open(
        match_file,
        RETRO_VFS_FILE_ACCESS_READ,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);

    if (!source)
    {
        self->loaded = false;
        return 1;
    }

    buffer = (uint8_t*)malloc((size_t)file_length);
    read_ok = read_exact(source, buffer, (size_t)file_length);
    filestream_close(source);

    if (!read_ok)
    {
        if (verbose && log_cb)
            log_cb(RETRO_LOG_ERROR,
                   "ROM has an unexpected size: %s\n",
                   match_file);

        free(buffer);
        self->loaded = false;
        return 1;
    }

    { int i; for (i = 0; i < file_length; i++)
        self->rom[(i * interleave) + offset] = buffer[i]; }

    if (verbose && log_cb)
        log_cb(RETRO_LOG_INFO,
               "Loaded renamed ROM by CRC32: %s\n",
               match_file);

    self->loaded = true;
    return 0;
}

int RomLoader_load_binary(RomLoader* self, const char* filename)
{
    bool read_ok;
    int64_t file_length;
    RFILE* source = filestream_open(
        filename,
        RETRO_VFS_FILE_ACCESS_READ,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);

    if (!source)
    {
        if (log_cb)
            log_cb(RETRO_LOG_ERROR, "Cannot open file: %s\n", filename);

        self->loaded = false;
        return 1;
    }

    file_length = filestream_get_size(source);
    if (file_length <= 0)
    {
        filestream_close(source);
        self->loaded = false;
        return 1;
    }

    RomLoader_unload(self);

    self->length = (uint32_t)(file_length);
    self->rom = (uint8_t*)malloc((self->length) * sizeof(uint8_t));

    read_ok = read_exact(source, self->rom, self->length);
    filestream_close(source);

    if (!read_ok)
    {
        RomLoader_unload(self);
        return 1;
    }

    self->loaded = true;
    return 0;
}

/* Load from an embedded memory buffer (no external file needed). */
int RomLoader_load_mem(RomLoader* self, const uint8_t* data, uint32_t len)
{
    if (!data || len == 0)
    {
        self->loaded = false;
        return 1;
    }

    RomLoader_unload(self);

    self->length = len;
    self->rom    = (uint8_t*)malloc((self->length) * sizeof(uint8_t));
    memcpy(self->rom, data, self->length);

    self->loaded = true;
    return 0;
}
