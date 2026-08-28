#ifndef UAE_SYSCONFIG_H
#define UAE_SYSCONFIG_H

#ifdef _MSC_VER
/* MSVC (VS2010) has no strcasecmp/strncasecmp/snprintf. libretro-common
 * already ships the mappings, so reuse them instead of patching every
 * call site. Must come first so the macros apply to all UAE sources.
 * snprintf maps to c99_snprintf_retro__ from
 * libretro-common/compat/compat_snprintf.c (added to the project). */
#include <compat/msvc.h>

#include <sys/types.h>
#include <sys/stat.h>
/* chmod() y close() viven aqui en MSVC. Sin esto el compilador las da por
 * implicitas (C4013, 'assuming extern returning int'), que en C89 significa
 * asumir una firma que puede no coincidir. */
#include <io.h>

/* POSIX permission bits: MSVC's <sys/stat.h> only has the _S_* owner names,
 * and its chmod() only honours read/write. Windows has no group/other
 * permissions, so those map to 0 (used with |= and & only). */
#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif
#ifndef S_IXUSR
#define S_IXUSR _S_IEXEC
#endif
#ifndef S_IRGRP
#define S_IRGRP 0
#define S_IWGRP 0
#define S_IXGRP 0
#define S_IROTH 0
#define S_IWOTH 0
#define S_IXOTH 0
#endif

/* autoconf would normally do this (see the 'mode_t' note further down). */
#ifndef mode_t
#define mode_t int
#endif

/* struct timeval lives in <winsock2.h>, which sysdeps.h deliberately keeps
 * out via WIN32_LEAN_AND_MEAN. Declare it here and set the winsock guard so
 * a later <winsock2.h> does not redefine it. gettimeofday() has no MSVC
 * equivalent at all; implemented in retrodep/machdep/support.c. */
#if !defined(_TIMEVAL_DEFINED) && !defined(_XBOX)
#define _TIMEVAL_DEFINED
struct timeval {
	long tv_sec;
	long tv_usec;
};
#endif
int gettimeofday(struct timeval *tv, void *tz);
#endif /* _MSC_VER */

#define DRIVESOUND
#define SUPPORT_THREADS
//#define OPTIMIZED_FLAGS

#define DEBUGGER
#define FILESYS /* filesys emulation */
#define UAE_FILESYS_THREADS
#define AUTOCONFIG /* autoconfig support, fast ram, harddrives etc.. */
/* JIT: solo en Windows. El backend es x86 (jit/codegen_x86.c,
 * jit/compemu_midfunc_x86.c), asi que en la 360 (PPC) no hay nada que compilar.
 * Con JIT activo, sysdeps.h define ademas NATMEM_OFFSET, y build_cpufunctbl()
 * pasa a poder elegir las tablas 40..45 / 50..55 (cpuemu_40.c / cpuemu_50.c).
 *
 * OJO: upstream libretro-uae NUNCA compila el JIT (el bloque esta comentado en
 * Makefile.common), asi que esta ruta se estrena aqui. Al proyecto se le han
 * anadido jit/compemu.c, jit/compemu_fpp.c, jit/compstbl.c,
 * jit/compemu_support.cpp y sources/src/vm.c, las cinco excluidas de las
 * configuraciones 'Xbox 360'.
 *
 * INTERRUPTOR: comentar este #define desactiva el JIT por completo sin tocar
 * el vcxproj. Quedan como objetos vacios (solo LNK4221) los cuatro ficheros de
 * jit/, por su #ifdef JIT, y cpuemu_40.c / cpuemu_50.c, a los que se les puso
 * guarda propia (gencpu no la emite). La unica excepcion es sources/src/vm.c,
 * que no depende del JIT y se sigue compilando: son cuatro funciones sin
 * llamantes, sin coste apreciable. */
//#ifndef _XBOX
//#define JIT /* JIT compiler support */
//#endif
//#define USE_JIT_FPU
//#define NOFLAGS_SUPPORT_GENCPU
//#define NOFLAGS_SUPPORT_GENCOMP
//#define HAVE_GET_WORD_UNSWAPPED
//#define NATMEM_OFFSET natmem_offset
#define USE_NORMAL_CALLING_CONVENTION 0
#define USE_X86_FPUCW 1
//#define CATWEASEL /* Catweasel MK2/3 support */
//#define AHI /* AHI sound emulation */
//#define ENFORCER /* UAE Enforcer */
#define ECS_DENISE /* ECS DENISE new features */
#define AGA /* AGA chipset emulation (ECS_DENISE must be enabled) */
#define CD32 /* CD32 emulation */
#define CDTV /* CDTV emulation */
#define PARALLEL_PORT /* parallel port emulation */
//#define PARALLEL_DIRECT /* direct parallel port emulation */
#define SERIAL_PORT /* serial port emulation */
//#define SERIAL_ENET /* serial port UDP transport */
#define SCSIEMU /* uaescsi.device emulation */
#define UAESERIAL /* uaeserial.device emulation */
#define FPUEMU /* FPU emulation */
#define MMUEMU /* Aranym 68040 MMU */
#define FULLMMU /* Aranym 68040 MMU */
/* ==================================================================== *
 *  MODELO DE AMIGA SOPORTADO                                           *
 *                                                                      *
 *  Define UNO solo. Determina que nucleos de CPU (cpuemu_NN.c) hay que  *
 *  compilar: los no seleccionados dejan de referenciarse desde          *
 *  cpustbl.c y newcpu.c, asi que no hace falta anadirlos al proyecto.   *
 * ==================================================================== */
/* En la Xbox 360 solo se compilan cpuemu_0/11/13: el vcxproj excluye los
 * demas de las dos configuraciones 'Xbox 360', porque son ~53.000 funciones
 * generadas de peso muerto en el .xex para un A500. En Windows entran todos. */
#ifdef _XBOX
#define UAE_MODEL_A500      /* A500/A500+/A600 - 68000/68010, OCS/ECS   */
//#define UAE_MODEL_A1200   /* A1200          - +68020, AGA             */
//#define UAE_MODEL_A4000   /* A4000/A3000    - +68030/040/060, FPU/MMU */
#else
//#define UAE_MODEL_A500    /* A500/A500+/A600 - 68000/68010, OCS/ECS   */
//#define UAE_MODEL_A1200   /* A1200          - +68020, AGA             */
#define UAE_MODEL_A4000     /* A4000/A3000    - +68030/040/060, FPU/MMU */
#endif

/* --- Precision de la emulacion 68000 (solo UAE_MODEL_A500) ----------- *
 *  Sin ninguno de los dos solo existe el modo "generic": es el mas
 *  rapido, pero no emula el prefetch del 68000 y eso rompe bastantes
 *  juegos y practicamente todas las demos.
 *  Al activarlos hay que anadir el cpuemu_NN.c correspondiente al
 *  proyecto (ver comentario de cada uno).                              */
#define UAE_CPU_COMPATIBLE   /* prefetch      -> compilar cpuemu_11.c */
#define UAE_CPU_CYCLE_EXACT  /* ciclo a ciclo -> compilar cpuemu_13.c */

/* -------- Derivacion de los CPUEMU_* a partir de lo anterior --------- */

/* cpuemu_0.c: nucleo generico. Genera las tablas 0..5, es decir el modo
 * "generic" de los seis modelos de CPU. Siempre necesario.             */
#define CPUEMU_0

#ifdef UAE_CPU_COMPATIBLE
#define CPUEMU_11 /* cpuemu_11.c: 68000/68010 prefetch      (tablas 11,12) */
#endif
#ifdef UAE_CPU_CYCLE_EXACT
#define CPUEMU_13 /* cpuemu_13.c: 68000/68010 cycle-exact   (tablas 13,14) */
#endif

#if defined(UAE_MODEL_A1200) || defined(UAE_MODEL_A4000)
#define CPUEMU_20 /* cpuemu_20.c: 68020 prefetch                          */
#define CPUEMU_21 /* cpuemu_21.c: 68020 cycle-exact + blitter             */
#endif

#ifdef UAE_MODEL_A4000
#define CPUEMU_22 /* cpuemu_22.c: 68030 prefetch                          */
#define CPUEMU_23 /* cpuemu_23.c: 68030 cycle-exact + blitter             */
#define CPUEMU_24 /* cpuemu_24.c: 68060 cycle-exact + blitter (tablas 24,25) */
#define CPUEMU_31 /* cpuemu_31.c: 68040 MMU                               */
#define CPUEMU_32 /* cpuemu_32.c: 68030 MMU                               */
#define CPUEMU_33 /* cpuemu_33.c: 68060 MMU                               */
#define CPUEMU_34 /* cpuemu_34.c: 68030 MMU + cache                       */
#define CPUEMU_35 /* cpuemu_35.c: 68030 MMU + cache + CE                  */
#endif

/* cpuemu_40.c (JIT, acceso directo -> tablas 40..45) y cpuemu_50.c (JIT,
 * acceso indirecto -> tablas 50..55). build_cpufunctbl() solo las elige
 * cuando currprefs.cachesize != 0, y cachesize solo se pone a MAX_JIT_CACHE
 * dentro de #ifdef JIT: sin JIT serian ~14.000 funciones inalcanzables.   */
#ifdef JIT
#define CPUEMU_40
#define CPUEMU_50
#endif

/* CPUEMU_68000_ONLY quitaria del build las instrucciones de 68020+ de
 * cpuemu_0.c y cpustbl.c, pero NO se puede activar: ademas de los
 * opcodes, recorta campos de 'struct regstruct' en newcpu.h (cacr, caar,
 * itt0/1, dtt0/1, tcr, mmusr, urp, srp, buscr, mmu_*, wb2_*, wb3_*) y
 * esos campos los usan sin guarda alguna newcpu_common.c (val_move2c /
 * val_move2c2), cpummu.c, cpummu30.c, debug.c y newcpu.c. Upstream nunca
 * lo define, y con el puesto el build da cientos de C2039.            */
//#define CPUEMU_68000_ONLY
#define ACTION_REPLAY /* Action Replay 1/2/3 support */
//#define PICASSO96 /* Picasso96 display card emulation */
//#define UAEGFX_INTERNAL /* built-in libs:picasso96/uaegfx.card */
//#define BSDSOCKET /* bsdsocket.library emulation */
#define CAPS /* CAPS-image support */
//#define SCP /* SuperCardPro */
#define FDI2RAW /* FDI 1.0 and 2.x image support */
//#define PROWIZARD /* Pro-Wizard module ripper */
#define ARCADIA /* Arcadia arcade system */
//#define ARCHIVEACCESS /* ArchiveAccess decompression library */
//#define LOGITECHLCD /* Logitech G15 LCD */
#define SAVESTATE /* State file support */
#define A2091 /* A590/A2091 SCSI */
//#define A2065 /* A2065 Ethernet card */
#define GFXBOARD /* Hardware graphics board */
#define NCR /* A4000T/A4091, 53C710/53C770 SCSI */
//#define NCR9X /* 53C9X SCSI */
//#define SANA2 /* SANA2 network driver */
//#define AMAX /* A-Max ROM adapter emulation */
//#define RETROPLATFORM /* Cloanto RetroPlayer support */
//#define WITH_LUA /* lua scripting */
//#define WITH_UAENATIVE
//#define WITH_SLIRP
//#define WITH_BUILTIN_SLIRP
//#define WITH_TABLETLIBRARY
//#define WITH_UAENET_PCAP
//#define WITH_PPC
//#define WITH_QEMU_CPU
//#define WITH_TOCCATA
//#define WITH_PCI
//#define WITH_X86
#define WITH_THREADED_CPU
//#define WITH_SOFTFLOAT
//#define FLOPPYBRIDGE
//#define WITH_MIDIEMU
//#define WITH_DSP
//#define WITH_DRACO

//#define WITH_CHD
#define A_ZIP
//#define A_RAR
//#define A_7Z
#define A_LHA
//#define A_LZX
#define A_DMS



/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* CPU supports 3DNOW */
/* #undef HAVE_3DNOW */

/* Define to 1 if you have the `alarm' function. */
//#define HAVE_ALARM 1

/* CPU supports AVX1 */
/* #undef HAVE_AVX */

/* CPU supports AVX2 */
/* #undef HAVE_AVX2 */

/* Define to 1 if you have the <caps/capsimage.h> header file. */
/* #undef HAVE_CAPS_CAPSIMAGE_H */

/* Define to 1 if you have the <curses.h> header file. */
//#define HAVE_CURSES_H 1

/* Define to 1 if you have the <cybergraphx/cybergraphics.h> header file. */
/* #undef HAVE_CYBERGRAPHX_CYBERGRAPHICS_H */

/* Define to 1 if you have the <devices/ahi.h> header file. */
/* #undef HAVE_DEVICES_AHI_H */

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* "Define to 1 if you have 'dlopen' function */
#if !defined(WIIU) && !defined(__SWITCH__) && !defined(VITA) && !defined(__PS3__)
#define HAVE_DLOPEN 1
#endif

/* Define to 1 if you have the <dustat.h> header file. */
/* #undef HAVE_DUSTAT_H */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the CAPS framework. */
/* #undef HAVE_FRAMEWORK_CAPSIMAGE */

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the `gmtime_r' function. */
//#define HAVE_GMTIME_R 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if <inttypes.h> exists, doesn't clash with <sys/types.h>, and
   declares uintmax_t. */
#define HAVE_INTTYPES_H_WITH_UINTMAX 1

/* Define to 1 if you have the `isinf' function. */
#define HAVE_ISINF 1

/* Define to 1 if you have the `isnan' function. */
#define HAVE_ISNAN 1

/* Define to 1 if you have the <libraries/cybergraphics.h> header file. */
/* #undef HAVE_LIBRARIES_CYBERGRAPHICS_H */

/* Define to 1 if you have the `localtime_r' function. */
//#define HAVE_LOCALTIME_R 1

/* Define to 1 if you have the <machine/joystick.h> header file. */
/* #undef HAVE_MACHINE_JOYSTICK_H */

/* Define to 1 if you have the <machine/soundcard.h> header file. */
/* #undef HAVE_MACHINE_SOUNDCARD_H */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* CPU supports MMX */
/* #undef HAVE_MMX */

/* Define to 1 if you have the `nanosleep' function. */
#ifndef VITA
#define HAVE_NANOSLEEP 1
#endif

/* Define to 1 if you have the <ncurses.h> header file. */
//#define HAVE_NCURSES_H 1

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the <pcap.h> header file. */
/* #undef HAVE_PCAP_H */

/* Define if you have POSIX threads libraries and header files. */
/* #undef HAVE_PTHREAD */
#define HAVE_PTHREAD 

/* Define to 1 if you have the `readdir_r' function. */
#define HAVE_READDIR_R 1

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* Define to 1 if you have the `setitimer' function. */
#if !defined(__SWITCH__) && !defined(VITA) && !defined(__PS3__)
#define HAVE_SETITIMER 1
#endif

/* Define to 1 if you have the `sigaction' function. */
#define HAVE_SIGACTION 1

/* Define to 1 if you have the <signal.h> header file. */
#if !defined(__PS3__) && !defined(_WIN32) && !defined(WIIU)
//#define HAVE_SIGNAL 1
#endif

/* Define to 1 if you have the `sleep' function. */
#define HAVE_SLEEP 1

/* CPU supports SSE */
/* #undef HAVE_SSE */

/* CPU supports SSE2 */
/* #undef HAVE_SSE2 */

/* CPU supports SSE3 */
/* #undef HAVE_SSE3 */

/* CPU supports SSE4A */
/* #undef HAVE_SSE4A */

/* CPU supports SSE4_1 */
/* #undef HAVE_SSE4_1 */

/* CPU supports SSE4_2 */
/* #undef HAVE_SSE4_2 */

/* CPU supports SSE5 */
/* #undef HAVE_SSE5 */

/* CPU supports SSSE3 */
/* #undef HAVE_SSSE3 */

/* Define to 1 if stdbool.h conforms to C99. */
#define HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define if <stdint.h> exists, doesn't clash with <sys/types.h>, and declares
   uintmax_t. */
#define HAVE_STDINT_H_WITH_UINTMAX 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define to 1 if you have the `strcmpi' function. */
/* #undef HAVE_STRCMPI */

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the `stricmp' function. */
/* #undef HAVE_STRICMP */

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Define to 1 if you have the `sync' function. */
#if !defined(__SWITCH__) && !defined(VITA) && !defined(__PS3__)
#define HAVE_SYNC 1
#endif

/* Define to 1 if you have the <sys/audioio.h> header file. */
/* #undef HAVE_SYS_AUDIOIO_H */

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/filio.h> header file. */
/* #undef HAVE_SYS_FILIO_H */

/* Define to 1 if you have the <sys/filsys.h> header file. */
/* #undef HAVE_SYS_FILSYS_H */

/* Define to 1 if you have the <sys/fs/s5param.h> header file. */
/* #undef HAVE_SYS_FS_S5PARAM_H */

/* Define to 1 if you have the <sys/fs_types.h> header file. */
/* #undef HAVE_SYS_FS_TYPES_H */

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/sysctl.h> header file. */
#ifdef __APPLE__
#define HAVE_SYS_SYSCTL_H 1
#endif

/* Define to 1 if you have the <sys/ipc.h> header file. */
//#define HAVE_SYS_IPC_H 1

/* Define to 1 if you have the <sys/mman.h> header file. */
//#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/mount.h> header file. */
//#define HAVE_SYS_MOUNT_H 1

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/param.h> header file. */
#ifndef __PS3__
#define HAVE_SYS_PARAM_H 1
#endif

/* Define to 1 if you have the <sys/shm.h> header file. */
//#define HAVE_SYS_SHM_H 1

/* Define to 1 if you have the <sys/soundcard.h> header file. */
//#define HAVE_SYS_SOUNDCARD_H 1

/* Define to 1 if you have the <sys/statfs.h> header file. */
//#define HAVE_SYS_STATFS_H 1

/* Define to 1 if you have the <sys/statvfs.h> header file. */
//#define HAVE_SYS_STATVFS_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/termios.h> header file. */
#define HAVE_SYS_TERMIOS_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/vfs.h> header file. */
//#define HAVE_SYS_VFS_H 1

/* Define to 1 if you have the `timegm' function. */
//#define HAVE_TIMEGM 1

/* Define if you have the 'uintmax_t' type in <stdint.h> or <inttypes.h>. */
#define HAVE_UINTMAX_T 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define if you have the 'unsigned long long' type. */
#define HAVE_UNSIGNED_LONG_LONG 1

/* Define to 1 if you have the `usleep' function. */
#define HAVE_USLEEP 1

/* Define to 1 if you have the <utime.h> header file. */
#define HAVE_UTIME_H 1

/* Define to 1 if `utime(file, NULL)' sets file's timestamp to the present. */
#define HAVE_UTIME_NULL 1

/* Define to 1 if you have the <values.h> header file. */
//#define HAVE_VALUES_H 1

/* Define to 1 if you have the `vfprintf' function. */
#define HAVE_VFPRINTF 1

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* Define to 1 if you have the `vsprintf' function. */
#define HAVE_VSPRINTF 1

/* Define to 1 if the system has the type `_Bool'. */
#define HAVE__BOOL 1

/* Version number of package */
#define VERSION "5.3.1"

/* Name of package */
#define PACKAGE "puae"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* cloned git commit version */
#define PACKAGE_COMMIT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "PUAE"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING PACKAGE_NAME " " VERSION

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "puae"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION VERSION

/* Define to the necessary symbol if this constant uses a non-standard name on
   your system. */
/* #undef PTHREAD_CREATE_JOINABLE */

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* The size of `char', as computed by sizeof. */
#define SIZEOF_CHAR 1

/* The size of `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* The size of `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* The size of `short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* The size of `void *', as computed by sizeof. */
#if defined(__SWITCH__) || defined(__x86_64__)
#define SIZEOF_VOID_P 8
#else
#define SIZEOF_VOID_P 4
#endif

/* The size of `__int64', as computed by sizeof. */
#define SIZEOF___INT64 0

/* Define if the block counts reported by statfs may be truncated to 2GB and
   the correct values may be stored in the f_spare array. (SunOS 4.1.2, 4.1.3,
   and 4.1.3_U1 are reported to have this problem. SunOS 4.1.1 seems not to be
   affected.) */
/* #undef STATFS_TRUNCATES_BLOCK_COUNTS */

/* Define if there is no specific function for reading filesystems usage
   information and you have the <sys/filsys.h> header file. (SVR2) */
/* #undef STAT_READ_FILSYS */

/* Define if statfs takes 2 args and struct statfs has a field named f_bsize.
   (4.3BSD, SunOS 4, HP-UX, AIX PS/2) */
//#define STAT_STATFS2_BSIZE 1

/* Define if statfs takes 2 args and struct statfs has a field named f_fsize.
   (4.4BSD, NetBSD) */
/* #undef STAT_STATFS2_FSIZE */

/* Define if statfs takes 2 args and the second argument has type struct
   fs_data. (Ultrix) */
/* #undef STAT_STATFS2_FS_DATA */

/* Define if statfs takes 3 args. (DEC Alpha running OSF/1) */
/* #undef STAT_STATFS3_OSF1 */

/* Define if statfs takes 4 args. (SVR3, Dynix, Irix, Dolphin) */
/* #undef STAT_STATFS4 */

/* Define if there is a function named statvfs. (SVR4) */
/* #undef STAT_STATVFS */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif


/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Define to 1 if the X Window System is missing or not being used. */
/* #undef X_DISPLAY_MISSING */

/* Number of bits in a file offset, on hosts where this is settable. */
#define _FILE_OFFSET_BITS 64

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to `int' if <sys/types.h> does not define. */
/* #undef mode_t */

/* Define to `long int' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Substitute for socklen_t */
/* #undef socklen_t */

/* Define to unsigned long or unsigned long long if <stdint.h> and
   <inttypes.h> don't define. */
/* #undef uintmax_t */

/* Define to empty if the keyword `volatile' does not work. Warning: valid
   code using `volatile' can become incorrect without. Disable with care. */
/* #undef volatile */

#ifdef MSB_FIRST
#  define bswap_16(x) (x)
#  define bswap_32(x) (x)
#elif defined(SWAP16) && defined(SWAP32)
#  define bswap_16(x) SWAP16(x)
#  define bswap_32(x) SWAP32(x)
#else
/* Otherwise, we'll roll our own. */
#  define bswap_16(x) (((x) >> 8) | (((x) & 0xFF) << 8))
#  define bswap_32(x) (((x) << 24) | (((x) << 8) & 0x00FF0000) | (((x) >> 8) & 0x0000FF00) | ((x) >> 24))
#endif

#ifdef USE_LIBRETRO_VFS
#include <streams/file_stream_transforms.h>
#undef stderr
#define stderr 0
#if !defined(WIN32) && !defined(_XBOX)
#undef HANDLE
#define HANDLE RFILE*
#endif
#undef putc
#define putc fputc
#undef fseeko
#define fseeko rfseek
#undef _fseeki64
#define _fseeki64 rfseek
#undef ftello
#define ftello rftell
#undef _ftelli64
#define _ftelli64 rftell
#else /* NO_LIBRETRO_VFS = 1 */
#if !defined(WIN32) && !defined(_XBOX)
#undef HANDLE
#define HANDLE FILE*
#endif
#ifndef _fseeki64
#define _fseeki64 fseeko64
#endif
#ifndef _ftelli64
#define _ftelli64 ftello64
#endif
#endif /* USE_LIBRETRO_VFS */

#ifndef PATH_MAX
#define PATH_MAX    256
#endif
//#ifndef MAX_PATH
//#define MAX_PATH	512
//#endif
#ifndef MAX_DPATH
#define MAX_DPATH	MAX_PATH
#endif

/* next */
#ifndef JIT
#define MIN_JIT_CACHE 128
#define MAX_JIT_CACHE 16384
#endif

#if defined(_MSC_VER) && (_MSC_VER < 1800)
#define strtoll _strtoi64

/* Las funciones de <math.h> de C99 no llegaron a MSVC hasta VS2013, y
 * fpp_native.c / fpp_softfloat.c las usan (tambien via los alias *l del
 * bloque __LIBRETRO__ de fpp_native.c: remainderl -> remainder, etc.).
 * Las cuatro primeras salen de lo que si trae <float.h>; las cinco
 * ultimas se implementan en retrodep/machdep/support.c.
 * Todas son macros con parametros, asi que no afectan a los
 * identificadores homonimos (fpp_native.c tiene un 'int round' local). */
#include <math.h>
#include <float.h>

#ifndef isnan
#define isnan(x)   (_isnan((double)(x)) != 0)
#endif
#ifndef isinf
#define isinf(x)   (!_finite((double)(x)) && !_isnan((double)(x)))
#endif
#ifndef signbit
#define signbit(x) (_copysign(1.0, (double)(x)) < 0.0)
#endif
#ifndef logb
#define logb(x)    _logb((double)(x))
#endif

extern double uae_msvc_round(double x);
extern double uae_msvc_remainder(double x, double y);
extern double uae_msvc_log1p(double x);
extern double uae_msvc_expm1(double x);
extern double uae_msvc_atanh(double x);

#ifndef round
#define round(x)        uae_msvc_round((double)(x))
#endif
#ifndef remainder
#define remainder(x, y) uae_msvc_remainder((double)(x), (double)(y))
#endif
#ifndef log1p
#define log1p(x)        uae_msvc_log1p((double)(x))
#endif
#ifndef expm1
#define expm1(x)        uae_msvc_expm1((double)(x))
#endif
#ifndef atanh
#define atanh(x)        uae_msvc_atanh((double)(x))
#endif
#endif

#if defined(WIN32) || defined(_XBOX)
#define FSDB_DIR_SEPARATOR '\\'
#define FSDB_DIR_SEPARATOR_S _T("\\")

#undef HAVE_UNISTD_H
#undef HAVE_UTIME_H
#undef TIME_WITH_SYS_TIME
#undef HAVE_SYS_TIME_H
#undef HAVE_SYS_PARAM_H
#undef HAVE_ISNAN
#undef HAVE_ISINF

#define STATIC_INLINE static __inline

#else
#define FSDB_DIR_SEPARATOR '/'
#define FSDB_DIR_SEPARATOR_S _T("/")
#endif

#endif /* UAE_SYSCONFIG_H */
