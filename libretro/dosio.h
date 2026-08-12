/* 
 * Copyright (c) 2003 NONAKA Kimihiro
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgment:
 *      This product includes software developed by NONAKA Kimihiro.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	__NP2_DOSIO_H__
#define	__NP2_DOSIO_H__

#include <stdint.h>
#include "common.h"

#define	FSEEK_SET 0
#define	FSEEK_CUR 1
#define	FSEEK_END 2

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_LIBRETRO_VFS
#include <file/file_path.h>
#include <retro_dirent.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

extern struct retro_vfs_interface_info vfs_iface_info;

#define STAT_IS_VALID     (1 << 0)
#define STAT_IS_DIRECTORY (1 << 1)

struct DIRH {
    struct RDIR *dir;
    const char *d_name;
};

typedef struct DIRH DIRH;
typedef struct RFILE FILEH;

int wrap_vfs_stat(const char *path, int32_t *size);
DIRH *wrap_vfs_opendir(const char *filename);
void wrap_vfs_closedir(DIRH *rdir);
int wrap_vfs_readdir(DIRH *rdir);
#endif

void *file_open(const char *filename);
void *file_create(const char *filename);
size_t file_seek(void *handle, long pointer, int16_t mode);
size_t file_lread(void *handle, void *data, size_t length);
size_t file_lwrite(void *handle, void *data, size_t length);
char *file_gets(void *handle, char *buffer, size_t length);
int64_t file_read(void *handle, void *buffer, int64_t length);
int64_t file_write(void *handle, void *buffer, int64_t length);
int64_t file_tell(void *handle);
void file_rewind(void *handle);
int file_eof(void *handle);
void file_close(void *handle);
void file_setcd(const char *exename);
void *file_open_c(const char *filename);
void *file_create_c(const char *filename);

#ifdef __cplusplus
};
#endif

#endif	/* __NP2_DOSIO_H__ */
