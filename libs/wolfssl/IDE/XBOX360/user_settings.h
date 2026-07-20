/* user_settings.h - Xbox 360 (PowerPC Big-Endian)
 * wolfSSL 5.8.4 configuration for curl with TLS 1.3 support
 * Based on IDE/WIN/user_settings.h and mbedTLS Xbox 360 build
 */

#ifndef _XBOX360_USER_SETTINGS_H_
#define _XBOX360_USER_SETTINGS_H_

/* ============================================================================
 * Platform detection
 * ============================================================================ */
#ifndef __ppc__
#define __ppc__
#endif
#ifndef _XBOX
#define _XBOX
#endif
#define BIG_ENDIAN_ORDER
#define SIZEOF_LONG       4
#define SIZEOF_LONG_LONG  8

/* ============================================================================
 * Build type
 * ============================================================================ */
#ifndef WOLFSSL_LIB
#define WOLFSSL_LIB
#endif
#ifndef WOLFSSL_USER_SETTINGS
#define WOLFSSL_USER_SETTINGS
#endif

/* ============================================================================
 * TLS versions
 * ============================================================================ */
#define WOLFSSL_TLS13
#define NO_OLD_TLS

/* ============================================================================
 * Cipher suites and key exchange
 * ============================================================================ */
#define HAVE_AESGCM
#define HAVE_AES_CBC
#define HAVE_CHACHA
#define HAVE_POLY1305
#define HAVE_ECC
#define ECC_SHAMIR
#define ECC_TIMING_RESISTANT
#define HAVE_CURVE25519
#define WC_RSA_PSS
#define WC_RSA_BLINDING

/* ============================================================================
 * TLS extensions (required for Firefox fingerprinting)
 * ============================================================================ */
#define HAVE_SUPPORTED_CURVES
#define HAVE_TLS_EXTENSIONS
/* La macro de wolfSSL es HAVE_EXTENDED_MASTER (HAVE_EXTENDED_MASTER_SECRET no
 * existe en wolfSSL). Activa RFC 7627 (extension extended_master_secret / ext 23,
 * que Firefox tambien envia) y quita el warning con secure renegotiation. */
#define HAVE_EXTENDED_MASTER
#define HAVE_SECURE_RENEGOTIATION
/* NO_WOLFSSL_SERVER blocks the auto-define of HAVE_SERVER_RENEGOTIATION_INFO in
 * settings.h, but wolfSSL_UseSecureRenegotiation/Rehandshake (used by curl) live
 * under it. Define it explicitly so those symbols get compiled in a client build. */
#define HAVE_SERVER_RENEGOTIATION_INFO
#define HAVE_ALPN
#define HAVE_SNI
#define HAVE_HKDF
#define HAVE_FFDHE_2048
#define WOLFSSL_SHA512

/* ============================================================================
 * OpenSSL compatibility API (enables wolfSSL_CTX_set1_groups_list, etc.)
 * ============================================================================ */
#define OPENSSL_EXTRA

/* Reanudacion de sesion: curl usa wolfSSL_i2d/d2i_SSL_SESSION (necesita HAVE_EXT_CACHE). */
#define HAVE_EXT_CACHE

/* ============================================================================
 * I/O layer - En el XDK no existen winsock2.h/ws2tcpip.h ni los headers POSIX de
 * socket, asi que wolfSSL NO debe compilar su capa de sockets: se usa
 * WOLFSSL_USER_IO + WOLFSSL_NO_SOCK (compila limpio, como el preset de ESP8266).
 * El I/O real (send/recv sobre los sockets XNet/WinSockX) se inyecta desde Salvia
 * con callbacks propios (wolfSSL_CTX_SetIORecv/SetIOSend via CURLOPT_SSL_CTX_FUNCTION),
 * porque curl usa wolfSSL_set_fd y EmbedSend/EmbedReceive no existen con NO_SOCK.
 * ============================================================================ */
#define WOLFSSL_USER_IO
#define WOLFSSL_NOT_WINDOWS_API
#define WOLFSSL_NO_SOCK
#define WOLFSSL_IGNORE_FILE_WARN
#define WOLFSSL_HAVE_MIN
#define WOLFSSL_HAVE_MAX

/* ============================================================================
 * Random seed - Xbox 360 via KeQueryPerformanceCounter
 * ============================================================================ */
/* wc_xbox360_GenerateSeed tiene firma de 3 args (OS_Seed*, byte*, word32), asi que
 * hay que usar la variante _OS. Con CUSTOM_RAND_GENERATE_SEED (2 args) wolfSSL la
 * llamaria como (output, sz) y los argumentos se desalinearian -> el seed escribe en
 * memoria invalida y wolfSSL_Init falla con WC_INIT_E (-228). Ver random.c ~L2681/2694. */
#define CUSTOM_RAND_GENERATE_SEED_OS wc_xbox360_GenerateSeed

/* ============================================================================
 * Footprint / Xbox 360 constraints
 * ============================================================================ */
#define WOLFSSL_SMALL_STACK
#define SINGLE_THREADED
#define SMALL_SESSION_CACHE
#define NO_MULTIBYTE_PRINT

/* ============================================================================
 * Disabled features (not available or not needed on Xbox 360)
 * ============================================================================ */
#define NO_FILESYSTEM
#define NO_WRITEV
#define NO_DEV_RANDOM
#define NO_DEV_URANDOM
/* NO_DSA / NO_DES3 / NO_SHA / NO_SESSION_CACHE quitados para alinear con Win32:
 * curl + OPENSSL_EXTRA referencia DSA, DES3, SHA-1 y las funciones de sesion; si
 * faltan aqui, el enlace de Salvia-Xbox daria unresolved externals. */
#define NO_RC4
#define NO_MD4
#define NO_PSK
#define NO_PSK_TLS13
#define NO_PWDBASED
#define NO_HC128
#define NO_RABBIT
#define NO_WOLFSSL_SERVER
#define NO_HANDSHAKE_SESSION_CACHE

/* ============================================================================
 * No hardware acceleration (x86/x64 only)
 * ============================================================================ */
#define NO_AESNI
#define WOLFSSL_NO_ASM
#define TFM_NO_ASM

/* ============================================================================
 * Math library
 * ============================================================================ */
#define USE_FAST_MATH
#define TFM_TIMING_RESISTANT
#define SP_INT_BITS 2048

/* ============================================================================
 * TLS 1.3 middlebox compatibility (RFC 8446 Appendix D.4)
 * ============================================================================ */
#define WOLFSSL_TLS13_MIDDLEBOX_COMPAT

#endif /* _XBOX360_USER_SETTINGS_H_ */
