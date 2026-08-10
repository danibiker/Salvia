/*
 *  1541t64.cpp - 1541 emulation in archive-type files (.t64/LYNX/.p00)
 *
 *  Frodo (C) 1994-1997,2002-2005 Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *  NOTES:
 *   - This module handles access to files inside (uncompressed) archives
 *     and makes the archive look like a disk. It supports C64S tape images
 *     (.t64), C64 LYNX archives and .p00 files.
 *   - If any file is opened, the contents of the file in the archive file are
 *     copied into a temporary file which is used for reading. This is done
 *     to insert the load address.
 *
 *  Incompatibilities:
 *   - Only read accesses possible
 *   - No "raw" directory reading
 *   - No relative/sequential/user files
 *   - Unimplemented commands: B-P, M-R, M-W, C, S, P, N
 *   - Impossible to implement: B-R, B-W, B-E, B-A, B-F, M-E
 */

#include <ctype.h>

#include "sysdeps.h"

#include "1541t64.h"
#include "IEC.h"
#include "Prefs.h"

#if defined(ANDROID) || defined(__ANDROID__)
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

int mkstemp2(char *path)
{
	int fd;   
	char *start, *trv;
	struct stat sbuf;
	pid_t pid = getpid();

	/* To guarantee multiple calls generate unique names even if
	   the file is not created. 676 different possibilities with 7
	   or more X's, 26 with 6 or less. */
	static char xtra[2] = {'a','a'};
	int xcnt = 0;

	/* Move to end of path and count trailing X's. */
	for (trv = path; *trv; ++trv)
		if (*trv == 'X')
			xcnt++;
		else
			xcnt = 0;	

	/* Use at least one from xtra.  Use 2 if more than 6 X's. */
	if (*(trv - 1) == 'X')
		*--trv = xtra[0];
	if (xcnt > 6 && *(trv - 1) == 'X')
		*--trv = xtra[1];

	/* Set remaining X's to pid digits with 0's to the left. */
	while (*--trv == 'X') {
		*trv = (pid % 10) + '0';
		pid /= 10;
	}

	/* update xtra for next call. */
	if (xtra[0] != 'z')
		xtra[0]++;
	else {
		xtra[0] = 'a';
		if (xtra[1] != 'z')
			xtra[1]++;
		else
			xtra[1] = 'a';
	}

	/*
	 * check the target directory; if you have six X's and it
	 * doesn't exist this runs for a *very* long time.
	 */
	for (start = trv + 1;; --trv) {
		if (trv <= path)
			break;
		if (*trv == '/') {
			*trv = '\0';
			if (stat(path, &sbuf))
				return (0);
			if ((sbuf.st_mode & S_IFDIR) != S_IFDIR) {
				errno = ENOTDIR;
				return (0);
			}
			*trv = '/';
			break;
		}
	}

	for (;;) {
		if ((fd=open(path, O_CREAT|O_EXCL|O_RDWR|O_BINARY, 0600)) >= 0){
			return (fd);
		}
		if (errno != EEXIST){
			return (0);
		}
		/* tricky little algorithm for backward compatibility */
		for (trv = start;;) {
			if (!*trv)
				return (0);
			if (*trv == 'z')
				*trv++ = 'a';
			else {
				if (isdigit((unsigned char)*trv))
					*trv = 'a';
				else
					++*trv;
				break;
			}
		}
	}
	/*NOTREACHED*/
}

class ScopedSignalBlocker {
 public:
  ScopedSignalBlocker() {
    sigset_t set;
    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, &old_set_);
  }

  ~ScopedSignalBlocker() {
    sigprocmask(SIG_SETMASK, &old_set_, NULL);
  }

 private:
  sigset_t old_set_;
};

static FILE* __tmpfile_dir(const char* tmp_dir)
{
   int fd, old_errno;
   char buf[PATH_MAX];
   ScopedSignalBlocker ssb;
   struct stat sb;
   FILE *fp        = NULL;
   int path_length = snprintf(buf, sizeof(buf), "%s/tmp.XXXXXXXXXX", tmp_dir);
   if (path_length >= sizeof(buf))
      return NULL;

   fd = mkstemp2(buf);
   if (fd == -1)
      return NULL;

   // Unlink the file now so that it's removed when closed.
   unlink(buf);

   // Can we still use the file now it's unlinked?
   // File systems without hard link support won't have the usual Unix semantics.
   int rc = fstat(fd, &sb);
   if (rc == -1)
   {
      old_errno     = errno;
      close(fd);
      errno         = old_errno;
      return NULL;
   }

   // Turn the file descriptor into a FILE*.
   fp = fdopen(fd, "w+");
   if (fp)
      return fp;

   // Failure. Clean up. We already unlinked, so we just need to close.
   old_errno = errno;
   close(fd);
   errno = old_errno;
   return NULL;
}

FILE* tmpfile2(void)
{
  FILE* fp = __tmpfile_dir("/data/data/com.retroarch/tmp");
  if (!fp)
     return __tmpfile_dir(P_tmpdir);
  return fp;
}

#endif

/* Forward declarations */
extern "C" {
RFILE* rfopen(const char *path, const char *mode);
int64_t rfseek(RFILE* stream, int64_t offset, int origin);
int64_t rftell(RFILE* stream);
int rfclose(RFILE* stream);
int64_t rfread(void* buffer,
   size_t elem_size, size_t elem_count, RFILE* stream);
int64_t rfwrite(void const* buffer,
   size_t elem_size, size_t elem_count, RFILE* stream);
int rfeof(RFILE* stream);
int rfscanf(RFILE * stream, const char * format, ...);
int rfgetc(RFILE* stream);
}

/* Prototypes */
static bool is_t64_header(const uint8 *header);
static bool is_lynx_header(const uint8 *header);
static bool is_p00_header(const uint8 *header);
static bool parse_t64_file(RFILE *f, std::vector<c64_dir_entry> &vec, char *dir_title);
static bool parse_lynx_file(RFILE *f, std::vector<c64_dir_entry> &vec, char *dir_title);
static bool parse_p00_file(RFILE *f, std::vector<c64_dir_entry> &vec, char *dir_title);

/*
 *  Constructor: Prepare emulation
 */

ArchDrive::ArchDrive(IEC *iec, const char *filepath) : Drive(iec), the_file(NULL)
{
   unsigned i;
	for (i = 0; i < 16; i++)
		file[i] = NULL;
	Reset();

	/* Open archive file */
	if (change_arch(filepath))
		Ready = true;
}

/* Destructor */
ArchDrive::~ArchDrive()
{
	// Close archive file
	if (the_file)
   {
      close_all_channels();
      rfclose(the_file);
   }
	Ready = false;
}


/* Open the archive file */
bool ArchDrive::change_arch(const char *path)
{
   RFILE *new_file;
   /* Open new archive file */
   if ((new_file = rfopen(path, "rb")))
   {
      uint8 header[64];
      bool parsed_ok = false;

      file_info.clear();

      // Read header, determine archive type and parse archive contents
      rfread(header, 1, 64, new_file);

      if (is_t64_header(header))
      {
         archive_type = TYPE_T64;
         parsed_ok    = parse_t64_file(new_file, file_info, dir_title);
      }
      else if (is_lynx_header(header))
      {
         archive_type = TYPE_LYNX;
         parsed_ok    = parse_lynx_file(new_file, file_info, dir_title);
      }
      else if (is_p00_header(header))
      {
         archive_type = TYPE_P00;
         parsed_ok    = parse_p00_file(new_file, file_info, dir_title);
      }

      if (!parsed_ok)
      {
         rfclose(new_file);
         if (the_file)
         {
            close_all_channels();
            rfclose(the_file);
            the_file = NULL;
         }
         return false;
      }

      // Close old archive if open, and set new file
      if (the_file)
      {
         close_all_channels();
         rfclose(the_file);
         the_file = NULL;
      }
      the_file = new_file;
      return true;
   }
   return false;
}


/*
 *  Open channel
 */

uint8 ArchDrive::Open(int channel, const uint8 *name, int name_len)
{
	set_error(ERR_OK);

	// Channel 15: Execute file name as command
	if (channel == 15)
   {
      execute_cmd(name, name_len);
      return ST_OK;
   }

	// Close previous file if still open
	if (file[channel])
   {
      fclose(file[channel]);
      file[channel] = NULL;
   }

	if (name[0] == '#')
   {
      set_error(ERR_NOCHANNEL);
      return ST_OK;
   }

	if (!the_file)
   {
      set_error(ERR_NOTREADY);
      return ST_OK;
   }

	if (name[0] == '$')
		return open_directory(channel, name + 1, name_len - 1);
	return open_file(channel, name, name_len);
}

/* Open file */
uint8 ArchDrive::open_file(int channel, const uint8 *name, int name_len)
{
   bool writing;
	uint8 plain_name[NAMEBUF_LENGTH];
	int plain_name_len;
	int mode = FMODE_READ;
	int type = FTYPE_DEL;
	int rec_len = 0;
	parse_file_name(name, name_len, plain_name, plain_name_len,
         mode, type, rec_len);

	// Channel 0 is READ, channel 1 is WRITE
	if (channel == 0 || channel == 1)
   {
      mode = channel ? FMODE_WRITE : FMODE_READ;
      if (type == FTYPE_DEL)
         type = FTYPE_PRG;
   }

	writing = (mode == FMODE_WRITE || mode == FMODE_APPEND);

	// Wildcards are only allowed on reading
	if (writing && (strchr((const char *)plain_name, '*') || strchr((const char *)plain_name, '?')))
   {
      set_error(ERR_SYNTAX33);
      return ST_OK;
   }

	// Allow only read accesses
	if (writing)
   {
      set_error(ERR_WRITEPROTECT);
      return ST_OK;
   }

	// Relative files are not supported
	if (type == FTYPE_REL)
   {
      set_error(ERR_UNIMPLEMENTED);
      return ST_OK;
   }

	// Find file
	int num;
	if (find_first_file(plain_name, plain_name_len, num))
   {
#if defined(ANDROID) || defined(__ANDROID__)
      if ((file[channel] = tmpfile2()))
#else
         // Open temporary file
         if ((file[channel] = tmpfile()))
#endif
         {
            // Write load address (.t64 only)
            if (archive_type == TYPE_T64)
            {
               fwrite(&file_info[num].sa_lo, 1, 1, file[channel]);
               fwrite(&file_info[num].sa_hi, 1, 1, file[channel]);
            }

            // Copy file contents from archive file to temp file
            uint8 *buf = new uint8[file_info[num].size];
            rfseek(the_file, file_info[num].offset, SEEK_SET);
            rfread(buf, file_info[num].size, 1, the_file);
            fwrite(buf, file_info[num].size, 1, file[channel]);
            rewind(file[channel]);
            delete[] buf;

            if (mode == FMODE_READ)	// Read and buffer first byte
               read_char[channel] = getc(file[channel]);
         }
   }
   else
		set_error(ERR_FILENOTFOUND);

	return ST_OK;
}


/*
 *  Find first file matching wildcard pattern
 */

// Return true if name 'n' matches pattern 'p'
static bool match(const uint8 *p, int p_len, const uint8 *n)
{
	while (p_len-- > 0)
   {
      if (*p == '*')	// Wildcard '*' matches all following characters
         return true;
      if ((*p != *n) && (*p != '?'))	// Wildcard '?' matches single character
         return false;
      p++; n++;
   }

	return *n == 0;
}

bool ArchDrive::find_first_file(const uint8 *pattern, int pattern_len, int &num)
{
	std::vector<c64_dir_entry>::const_iterator i, end = file_info.end();
	for (i = file_info.begin(), num = 0; i != end; i++, num++)
   {
      if (match(pattern, pattern_len, (uint8 *)i->name))
         return true;
   }
	return false;
}


/*
 *  Open directory, create temporary file
 */

uint8 ArchDrive::open_directory(int channel, const uint8 *pattern, int pattern_len)
{
   uint8 *t;
	// Special treatment for "$0"
	if (pattern[0] == '0' && pattern_len == 1)
   {
      pattern++;
      pattern_len--;
   }

	// Skip everything before the ':' in the pattern
	t = (uint8 *)memchr(pattern, ':', pattern_len);
	if (t)
   {
      t++;
      pattern_len -= t - pattern;
      pattern = t;
   }

#if defined(ANDROID) || defined(__ANDROID__)
	// Create temporary file
	if ((file[channel] = tmpfile2()) == NULL)
		return ST_OK;
#else
	// Create temporary file
	if ((file[channel] = tmpfile()) == NULL)
		return ST_OK;
#endif

	// Create directory title
	uint8 buf[] = "\001\004\001\001\0\0\022\042                \042 00 2A";
	for (int i=0; i<16 && dir_title[i]; i++)
		buf[i + 8] = dir_title[i];
	fwrite(buf, 1, 32, file[channel]);

	// Create and write one line for every directory entry
	std::vector<c64_dir_entry>::const_iterator i, end = file_info.end();
	for (i = file_info.begin(); i != end; i++) {

		// Include only files matching the pattern
		if (pattern_len == 0 || match(pattern, pattern_len, (uint8 *)i->name)) {

			// Clear line with spaces and terminate with null byte
			memset(buf, ' ', 31);
			buf[31] = 0;

			uint8 *p = (uint8 *)buf;
			*p++ = 0x01;	// Dummy line link
			*p++ = 0x01;

			// Calculate size in blocks (254 bytes each)
			int n = (i->size + 254) / 254;
			*p++ = n & 0xff;
			*p++ = (n >> 8) & 0xff;

			p++;
			if (n < 10) p++;	// Less than 10: add one space
			if (n < 100) p++;	// Less than 100: add another space

			// Convert and insert file name
			*p++ = '\"';
			uint8 *q = p;
			for (int j=0; j<16 && i->name[j]; j++)
				*q++ = i->name[j];
			*q++ = '\"';
			p += 18;

			// File type
			switch (i->type) {
				case FTYPE_DEL:
					*p++ = 'D';
					*p++ = 'E';
					*p++ = 'L';
					break;
				case FTYPE_SEQ:
					*p++ = 'S';
					*p++ = 'E';
					*p++ = 'Q';
					break;
				case FTYPE_PRG:
					*p++ = 'P';
					*p++ = 'R';
					*p++ = 'G';
					break;
				case FTYPE_USR:
					*p++ = 'U';
					*p++ = 'S';
					*p++ = 'R';
					break;
				case FTYPE_REL:
					*p++ = 'R';
					*p++ = 'E';
					*p++ = 'L';
					break;
				default:
					*p++ = '?';
					*p++ = '?';
					*p++ = '?';
					break;
			}

			// Write line
			fwrite(buf, 1, 32, file[channel]);
		}
	}

	// Final line
	fwrite("\001\001\0\0BLOCKS FREE.             \0\0", 1, 32, file[channel]);

	// Rewind file for reading and read first byte
	rewind(file[channel]);
	read_char[channel] = getc(file[channel]);

	return ST_OK;
}


/*
 *  Close channel
 */

uint8 ArchDrive::Close(int channel)
{
	if (channel == 15) {
		close_all_channels();
		return ST_OK;
	}

	if (file[channel]) {
		fclose(file[channel]);
		file[channel] = NULL;
	}

	return ST_OK;
}


/*
 *  Close all channels
 */

void ArchDrive::close_all_channels(void)
{
	for (int i=0; i<15; i++)
		Close(i);

	cmd_len = 0;
}


/*
 *  Read from channel
 */

uint8 ArchDrive::Read(int channel, uint8 &byte)
{
	// Channel 15: Error channel
	if (channel == 15) {
		byte = *error_ptr++;

		if (byte != '\x0d')
			return ST_OK;
		else {	// End of message
			set_error(ERR_OK);
			return ST_EOF;
		}
	}

	if (!file[channel]) return ST_READ_TIMEOUT;

	// Get char from buffer and read next
	byte = read_char[channel];
	int c = getc(file[channel]);
	if (c == EOF)
		return ST_EOF;
	else {
		read_char[channel] = c;
		return ST_OK;
	}
}


/*
 *  Write to channel
 */

uint8 ArchDrive::Write(int channel, uint8 byte, bool eoi)
{
	// Channel 15: Collect chars and execute command on EOI
	if (channel == 15) {
		if (cmd_len > 58) {
			set_error(ERR_SYNTAX32);
			return ST_TIMEOUT;
		}
		
		cmd_buf[cmd_len++] = byte;

		if (eoi) {
			execute_cmd(cmd_buf, cmd_len);
			cmd_len = 0;
		}
		return ST_OK;
	}

	if (!file[channel])
		set_error(ERR_FILENOTOPEN);
	else
		set_error(ERR_WRITEPROTECT);

	return ST_TIMEOUT;
}


/*
 *  Execute drive commands
 */

// RENAME:new=old
//        ^   ^
// new_file   old_file
void ArchDrive::rename_cmd(const uint8 *new_file, int new_file_len, const uint8 *old_file, int old_file_len)
{
	// Check if destination file is already present
	int num;
	if (find_first_file(new_file, new_file_len, num)) {
		set_error(ERR_FILEEXISTS);
		return;
	}

	// Check if source file is present
	if (!find_first_file(old_file, old_file_len, num)) {
		set_error(ERR_FILENOTFOUND);
		return;
	}

	set_error(ERR_WRITEPROTECT);
}

// INITIALIZE
void ArchDrive::initialize_cmd(void)
{
	close_all_channels();
}

// VALIDATE
void ArchDrive::validate_cmd(void)
{
}


/*
 *  Reset drive
 */

void ArchDrive::Reset(void)
{
	close_all_channels();
	cmd_len = 0;	
	set_error(ERR_STARTUP);
}


/*
 *  Check whether file with given header (64 bytes) and size looks like one
 *  of the file types supported by this module
 */

static bool is_t64_header(const uint8 *header)
{
	if (memcmp(header, "C64S tape file", 14) == 0
	 || memcmp(header, "C64 tape image", 14) == 0
	 || memcmp(header, "C64S tape image", 15) == 0)
		return true;
   return false;
}

static bool is_lynx_header(const uint8 *header)
{
	return memcmp(header + 0x38, "USE LYNX", 8) == 0;
}

static bool is_p00_header(const uint8 *header)
{
	return memcmp(header, "C64File", 7) == 0;
}

bool IsArchFile(const char *path, const uint8 *header, long size)
{
	return is_t64_header(header) || is_lynx_header(header) || is_p00_header(header);
}


/*
 *  Read directory of archive file into (empty) c64_dir_entry vector,
 *  returns false on error
 */

static bool parse_t64_file(RFILE *f, std::vector<c64_dir_entry> &vec, char *dir_title)
{
	uint8 buf[32];
   int max, i;
	int num_files = 0;
	// Read header and get maximum number of files contained
	rfseek(f, 32, SEEK_SET);
	rfread(&buf, 32, 1, f);
	max = (buf[3] << 8) | buf[2];
	if (max == 0)
		max = 1;

	memcpy(dir_title, buf+8, 16);

	// Allocate buffer for file records and read them
	uint8 *buf2 = new uint8[max * 32];
	rfread(buf2, 32, max, f);

	// Determine number of files contained
	for (i = 0; i < max; i++)
   {
      if (buf2[i*32] == 1)
         num_files++;
   }

	if (!num_files)
   {
      delete[] buf2;
      return false;
   }

	// Construct file information array
	vec.reserve(num_files);
	uint8 *b = buf2;
	for (int i=0; i<max; i++, b+=32)
   {
      if (b[0] == 1)
      {
         // Convert file name (strip trailing spaces)
         uint8 name_buf[17];
         memcpy(name_buf, b + 16, 16);
         name_buf[16] = 0x20;
         uint8 *p = name_buf + 16;
         while (*p-- == 0x20) ;
         p[2] = 0;

         // Find file size and offset
         size_t size = ((b[5] << 8) | b[4]) - ((b[3] << 8) | b[2]);
         off_t offset = (b[11] << 24) | (b[10] << 16) | (b[9] << 8) | b[8];

         // Add entry
         vec.push_back(c64_dir_entry(name_buf, FTYPE_PRG, false, false, size, offset, b[2], b[3]));
      }
   }

	delete[] buf2;
	return true;
}

static bool parse_lynx_file(RFILE *f, std::vector<c64_dir_entry> &vec, char *dir_title)
{
	int dir_blocks;
	int num_files;
	// Dummy directory title
	strcpy(dir_title, "LYNX ARCHIVE    ");

	// Read header and get number of directory blocks and files contained
	rfseek(f, 0x60, SEEK_SET);
	rfscanf(f, "%d", &dir_blocks);
	while (rfgetc(f) != 0x0d)
		if (rfeof(f))
			return false;
	rfscanf(f, "%d\x0d", &num_files);

	// Construct file information array
	vec.reserve(num_files);
	int cur_offset = dir_blocks * 254;
	for (int i=0; i<num_files; i++)
   {
      uint8 *p;
      uint8 sa_lo, sa_hi;
      uint8 name_buf[17];
      int num_blocks, last_block;
      char type_char;
      size_t size;
      int type;
      int64_t here;

      // Read and convert file name (strip trailing shift-spaces)
      rfread(name_buf, 16, 1, f);
      name_buf[16] = 0xa0;
      p            = name_buf + 16;
      while (*p-- == 0xa0) ;
      p[2]         = 0;

      // Read file length and type
      rfscanf(f, "\x0d%d\x0d%c\x0d%d\x0d", &num_blocks, &type_char, &last_block);
      size = (num_blocks - 1) * 254 + last_block - 1;

      switch (type_char)
      {
         case 'S':
            type = FTYPE_SEQ;
            break;
         case 'U':
            type = FTYPE_USR;
            break;
         case 'R':
            type = FTYPE_REL;
            break;
         default:
            type = FTYPE_PRG;
            break;
      }

      // Read start address
      here = rftell(f);
      rfseek(f, cur_offset, SEEK_SET);
      rfread(&sa_lo, 1, 1, f);
      rfread(&sa_hi, 1, 1, f);
      rfseek(f, here, SEEK_SET);

      // Add entry
      vec.push_back(c64_dir_entry(name_buf, type, false, false, size, cur_offset, sa_lo, sa_hi));

      cur_offset += num_blocks * 254;
   }

	return true;
}

static bool parse_p00_file(RFILE *f, std::vector<c64_dir_entry> &vec, char *dir_title)
{
	uint8 name_buf[17];
	uint8 sa_lo, sa_hi;
   int64_t size;

	// Dummy directory title
	strcpy(dir_title, ".P00 FILE       ");

	// Contains only one file
	vec.reserve(1);

	// Read file name and start address
	rfseek(f, 8, SEEK_SET);
	rfread(name_buf, 17, 1, f);
	name_buf[16] = 0;
	rfseek(f, 26, SEEK_SET);
	rfread(&sa_lo, 1, 1, f);
	rfread(&sa_hi, 1, 1, f);

	// Get file size
	rfseek(f, 0, SEEK_END);
	size = rftell(f) - 26;

	// Add entry
	vec.push_back(c64_dir_entry(name_buf, FTYPE_PRG, false, false, size, 26, sa_lo, sa_hi));
	return true;
}

bool ReadArchDirectory(const char *path, std::vector<c64_dir_entry> &vec)
{
   // Open file
   RFILE *f = rfopen(path, "rb");
   if (f)
   {
      // Read header
      bool result = false;
      char dir_title[16];
      uint8 header[64];
      rfread(header, 1, sizeof(header), f);

      // Determine archive type and parse archive
      if (is_t64_header(header))
         result = parse_t64_file(f, vec, dir_title);
      else if (is_lynx_header(header))
         result = parse_lynx_file(f, vec, dir_title);
      else if (is_p00_header(header))
         result = parse_p00_file(f, vec, dir_title);

      rfclose(f);
      return result;
   }
   return false;
}
