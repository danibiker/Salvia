/*
 * disc_chd.c - .chd reader for the 3dox libretro core.
 *
 * Uses opera-libretro's chd_stream.c (proven on Xbox 360) layered over the
 * bundled deps/libchdr. chd_stream presents the CHD as a flat "full disc" byte
 * stream and internally handles the per-track frame size, pregap and padding
 * (the subtle parts a hand-rolled reader gets wrong on multi-track discs like
 * Road Rash). On top of it we replicate opera's retro_cdimage read model:
 *
 *   - detect the user-data offset from the first sector's content
 *     (3DO volume header 01 5A 5A 5A 5A 5A 01 00 => cooked, offset 0, stride
 *     2448; otherwise raw Mode 1 => offset 16, stride 2352), and
 *   - return sector N's 2048 user bytes from  N*stride + offset.
 *
 * This is exactly what opera does, so any disc that opens in opera opens here.
 *
 * Kept in its own C translation unit so chd_stream / libchdr headers never
 * reach the C++ shim (libretro.cpp), which uses only the extern "C" API below.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <streams/chd_stream.h>
#include <libchdr/chd.h>

/* Logging hook implemented in libretro.cpp (routes to the libretro logger). */
extern void cd3do_log(const char *msg);

#define THREEDO_USER_DATA 2048

static chdstream_t *s_stream      = NULL;
static int          s_sector_size = 2352;   /* on-stream stride: 2352 (raw) or 2448 (cooked) */
static int          s_offset      = 16;     /* user-data offset within the sector: 0 / 16 / 24 */
static int          s_total       = 0;      /* number of logical sectors */

void cd3do_chd_close(void)
{
   if (s_stream)
   {
      chdstream_close(s_stream);
      s_stream = NULL;
   }
   s_sector_size = 2352;
   s_offset      = 16;
   s_total       = 0;
}

/* Returns the number of logical sectors on success, <= 0 on failure. */
int cd3do_chd_open(const char *path)
{
   static const unsigned char vol3do[8] =
      { 0x01,0x5a,0x5a,0x5a,0x5a,0x5a,0x01,0x00 };
   static const unsigned char cdsync[12] =
      { 0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00 };
   unsigned char hdr[32];
   ssize_t       total_bytes;
   char          b[192];

   cd3do_chd_close();

   s_stream = chdstream_open(path, CHDSTREAM_TRACK_FULL_DISC);
   if (!s_stream)
      return -1;

   /* Read the first sector's header from the flat disc stream and pick the
    * user-data offset by content (metadata track-mode alone is unreliable). */
   chdstream_seek(s_stream, 0, SEEK_SET);
   if (chdstream_read(s_stream, hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr))
   {
      cd3do_chd_close();
      return -2;
   }
   chdstream_seek(s_stream, 0, SEEK_SET);

   if (memcmp(hdr, vol3do, 8) == 0)
   {
      s_sector_size = 2448;                  /* cooked: 3DO volume header @0 */
      s_offset      = 0;
   }
   else if (memcmp(hdr, cdsync, 12) == 0)
   {
      s_sector_size = 2352;                  /* raw CD sector */
      s_offset      = (hdr[15] == 2) ? 24 : 16;
   }
   else
   {
      s_sector_size = 2352;                  /* fall back to raw Mode 1 */
      s_offset      = 16;
   }

   total_bytes = chdstream_get_size(s_stream);
   if (total_bytes <= 0)
   {
      cd3do_chd_close();
      return -3;
   }
   s_total = (int)(total_bytes / s_sector_size);

   /* Multi-track discs (e.g. Road Rash, with CDDA audio tracks) concatenate every
      track in the flat full-disc stream, so total_bytes/stride includes the audio.
      The 3DO only boots from the DATA track; advertising a disc bigger than the
      data volume -- or off by the standard 150-frame pregap -- breaks the OS's
      disc-label sanity check, so the BIOS menu appears despite reads being fine.

      Enumerate the tracks with chdstream_get_cdrom_metadata (robust: it parses
      pregap/pgtype for us, unlike the old hand-rolled sscanf that silently failed
      to match some dumps and left the full-disc size in place -> BIOS) and report
      just the first DATA track's frame count, pregap-adjusted exactly like opera's
      cdimage_chd_data_frames (retro_cdimage.c). Single-track discs (e.g. GEX) are
      unaffected: their data-track frame count == the full-disc frame count. */
   {
      chd_file *mchd = NULL;
      if (chd_open(path, CHD_OPEN_READ, NULL, &mchd) == CHDERR_NONE)
      {
         chdstream_cdrom_metadata_t md;
         int idx, ntracks = 0, data_total = -1;

         for (idx = 0; idx < 99; idx++)
         {
            unsigned data_frames;
            if (!chdstream_get_cdrom_metadata(mchd, idx, &md))
               break;
            ntracks++;
            /* An in-track ('V') pregap is part of the frame count and lives in
               the stream, so it must be subtracted to get the real data frames;
               other pregap types are not present in the stream. */
            if (md.pgtype[0] == 'V')
               data_frames = (md.frames > md.pregap) ? (md.frames - md.pregap) : 0;
            else
               data_frames = md.frames;
            if (data_total < 0 && strncmp(md.type, "AUDIO", 5) != 0)
               data_total = (int)data_frames;   /* first DATA track */
         }
         chd_close(mchd);

         if (ntracks > 1 && data_total > 0)
            s_total = data_total;   /* drop the audio tracks (pregap-adjusted) */
      }
   }

   sprintf(b, "chd opened: %d sectors, stride=%d, data offset=%d",
           s_total, s_sector_size, s_offset);
   cd3do_log(b);

   return s_total;
}

/* Read the 2048-byte user data of logical sector 'lba'. Returns bytes read. */
long cd3do_chd_read(unsigned lba, void *buf, unsigned bufsize)
{
   if (!s_stream)
      return -1;
   if (bufsize < THREEDO_USER_DATA)
      return -1;
   if ((int)lba >= s_total)
      return -1;

   if (chdstream_seek(s_stream,
                      (int64_t)lba * s_sector_size + s_offset,
                      SEEK_SET) < 0)
      return -1;

   return (long)chdstream_read(s_stream, buf, THREEDO_USER_DATA);
}
