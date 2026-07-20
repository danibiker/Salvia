/* user_settings.h - Win32 (x86 Little-Endian)
 * wolfSSL 5.8.4 configuration for curl with TLS 1.3 support
 */

#ifndef _WIN32_USER_SETTINGS_H_
#define _WIN32_USER_SETTINGS_H_

/* Include stdio.h early to prevent vsnprintf linkage conflict with MSVC 2010 */
#include <stdio.h>

#ifndef WOLFSSL_LIB
#define WOLFSSL_LIB
#endif
#ifndef WOLFSSL_USER_SETTINGS
#define WOLFSSL_USER_SETTINGS
#endif

/* TLS versions */
#define WOLFSSL_TLS13
#define NO_OLD_TLS

/* Cipher suites and key exchange */
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

/* TLS extensions */
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

/* OpenSSL compatibility API */
#define OPENSSL_EXTRA

/* Session resumption: curl uses wolfSSL_i2d/d2i_SSL_SESSION (needs HAVE_EXT_CACHE)
 * and wolfSSL_set_session (needs the session cache, i.e. NOT NO_SESSION_CACHE) */
#define HAVE_EXT_CACHE

/* I/O layer: curl integra wolfSSL con wolfSSL_set_fd(), que usa el I/O interno
 * (EmbedSend/EmbedReceive, compilados por USE_WOLFSSL_IO). NO definir
 * WOLFSSL_USER_IO: dejaria esos callbacks a NULL y el handshake fallaria con
 * -308 SOCKET_ERROR_E ("error state on socket"). */
#define USE_WOLFSSL_IO
/* #define WOLFSSL_USER_IO */
#define WOLFSSL_IGNORE_FILE_WARN
#define WOLFSSL_HAVE_MIN
#define WOLFSSL_HAVE_MAX

/* Footprint */
#define SINGLE_THREADED
#define SMALL_SESSION_CACHE

/* Disabled features */
#define NO_FILESYSTEM
#define NO_WRITEV
#define NO_DEV_RANDOM
#define NO_DEV_URANDOM
#define NO_RC4
#define NO_MD4
#define NO_PSK
#define NO_PSK_TLS13
#define NO_PWDBASED
#define NO_HC128
#define NO_RABBIT
#define NO_WOLFSSL_SERVER
/* NO_SESSION_CACHE removed: curl needs the session cache for TLS resumption
 * (wolfSSL_set_session / wolfSSL_get1_session). SMALL_SESSION_CACHE keeps it tiny. */
#define NO_HANDSHAKE_SESSION_CACHE

/* No hardware acceleration */
#define NO_AESNI
#define WOLFSSL_NO_ASM
#define TFM_NO_ASM

/* Math library */
#define USE_FAST_MATH
#define TFM_TIMING_RESISTANT
#define SP_INT_BITS 2048

/* TLS 1.3 middlebox compatibility */
#define WOLFSSL_TLS13_MIDDLEBOX_COMPAT

#endif /* _WIN32_USER_SETTINGS_H_ */
