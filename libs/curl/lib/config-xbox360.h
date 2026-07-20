#ifndef HEADER_CURL_CONFIG_XBOX360_H
#define HEADER_CURL_CONFIG_XBOX360_H

/* Hand-crafted config for Xbox 360 with wolfSSL */

/* Override _WIN32_WINNT to satisfy curl 8.22 minimum check */
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600

/* ---------------------------------------------------------------- */
/*              DISABLE UNICODE (XDK lacks multibyte APIs)          */
/* ---------------------------------------------------------------- */
#ifdef UNICODE
#undef UNICODE
#endif
#ifdef _UNICODE
#undef _UNICODE
#endif

/* ---------------------------------------------------------------- */
/*                       FEATURE DISABLING                          */
/* ---------------------------------------------------------------- */
#ifndef HTTP_ONLY
#define HTTP_ONLY
#endif
#define CURL_DISABLE_LDAP
#define CURL_DISABLE_LDAPS
#define CURL_DISABLE_DOH
#define CURL_DISABLE_WEBSOCKETS
#define CURL_DISABLE_TCP_KEEPALIVE 0


/* UWP path uses simpler version detection (no VER_* / VerifyVersionInfoW) */
#define CURL_WINDOWS_UWP

/* ---------------------------------------------------------------- */
/*                          HEADER FILES                            */
/* ---------------------------------------------------------------- */

#define HAVE_FCNTL_H 1
#define HAVE_IO_H 1
#define HAVE_LOCALE_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_UTIME_H 1

/* ---------------------------------------------------------------- */
/*                        OTHER HEADER INFO                         */
/* ---------------------------------------------------------------- */

/* ---------------------------------------------------------------- */
/*                             FUNCTIONS                            */
/* ---------------------------------------------------------------- */

#define HAVE_CLOSESOCKET 1
#define HAVE_GETPEERNAME 1
#define HAVE_GETSOCKNAME 1
#define HAVE_GETHOSTNAME 1
#define HAVE_IOCTLSOCKET 1
#define HAVE_IOCTLSOCKET_FIONBIO 1
#define HAVE_SELECT 1
#define HAVE_SETLOCALE 1
#define HAVE_SOCKET 1
#define HAVE_UTIME 1
#define HAVE_RECV 1
#define HAVE_SEND 1
#define HAVE_SIGNAL 1
#define HAVE_STDBOOL_H 0

/* getaddrinfo: prevents Curl_he2ai / struct hostent usage */
#define HAVE_GETADDRINFO 1
#define HAVE_GETADDRINFO_THREADSAFE 1

/* XDK provides struct timeval via xtl.h/winsockx.h */
#define HAVE_STRUCT_TIMEVAL 1

/* XDK winsockx.h is minimal - define missing winsock constants */
#ifdef _XBOX
#ifndef SO_KEEPALIVE
#define SO_KEEPALIVE 0x1002
#define HAVE_WOLFSSL_BIO_NEW
#endif
#ifndef IOC_VENDOR
#define IOC_VENDOR 0x18000000
#endif
#endif /* _XBOX */

/* recv/send type signatures */
#define RECV_TYPE_ARG1 SOCKET
#define RECV_TYPE_ARG2 char *
#define RECV_TYPE_ARG3 int
#define RECV_TYPE_ARG4 int
#define RECV_TYPE_RETV int

#define SEND_TYPE_ARG1 SOCKET
#define SEND_TYPE_ARG2 char *
#define SEND_TYPE_ARG3 int
#define SEND_TYPE_ARG4 int
#define SEND_TYPE_RETV int

/* ---------------------------------------------------------------- */
/*                       TYPEDEF REPLACEMENTS                       */
/* ---------------------------------------------------------------- */

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
#define ssize_t int
#endif

/* ADDRESS_FAMILY - XDK winsockx.h doesn't provide it */
#ifndef _ADDRESS_FAMILY_DEFINED
#define _ADDRESS_FAMILY_DEFINED
typedef unsigned short ADDRESS_FAMILY;
#endif

/* ---------------------------------------------------------------- */
/*               STUBS FOR MISSING WS2TCPIP TYPES                   */
/* ---------------------------------------------------------------- */
#ifdef _XBOX

/* struct addrinfo - XDK lacks ws2tcpip.h */
#ifndef _ADDRINFO_DEFINED
#define _ADDRINFO_DEFINED
struct addrinfo {
  int              ai_flags;
  int              ai_family;
  int              ai_socktype;
  int              ai_protocol;
  size_t           ai_addrlen;
  char            *ai_canonname;
  struct sockaddr *ai_addr;
  struct addrinfo *ai_next;
};
#endif

#define EAI_MEMORY    11004
#define EAI_NODATA    11001

/* getaddrinfo/freeaddrinfo - implemented in xbox360_compat.c */
int getaddrinfo(const char *nodename, const char *servname,
                const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);

/* struct hostent - XDK winsockx.h does not provide it */
#ifndef _HOSTENT_DEFINED
#define _HOSTENT_DEFINED
struct hostent {
  char  *h_name;
  char **h_aliases;
  short  h_addrtype;
  short  h_length;
  char **h_addr_list;
};
#define h_addr h_addr_list[0]
#endif

/* gethostname - implemented in xbox360_compat.c */
int WSAAPI gethostname(char *name, int namelen);

/* gethostbyname - implemented in xbox360_compat.c */
struct hostent *gethostbyname(const char *name);

/* ---------------------------------------------------------------- */
/*              STUBS FOR MISSING WINSOCK2 TYPES                    */
/* ---------------------------------------------------------------- */

/* FD_OOB - missing from XDK's winsockx.h */
#ifndef FD_OOB
#define FD_OOB 0x0004
#endif

/* WSAIoctl - used for TCP keepalive on Windows; stub it out */
#ifndef _WSAIOW
#define _WSAIOW(x,y) (x | y)
#endif

#ifndef SIO_KEEPALIVE_VALS
#define SIO_KEEPALIVE_VALS _WSAIOW(IOC_VENDOR, 4)
#endif

/* tcp_keepalive struct - used with SIO_KEEPALIVE_VALS */
#ifndef _TCP_KEEPALIVE_DEFINED
#define _TCP_KEEPALIVE_DEFINED
struct tcp_keepalive {
  unsigned long onoff;
  unsigned long keepalivetime;
  unsigned long keepaliveinterval;
};
#endif

/* ---------------------------------------------------------------- */
/*                  STUBS FOR MISSING WIN32 APIS                    */
/* ---------------------------------------------------------------- */

/* SRWLOCK - needed by easy_lock.h; XDK has no Windows Vista+ synchapi */
#ifndef _SRWLOCK_DEFINED
#define _SRWLOCK_DEFINED
typedef struct _SRWLOCK {
  void *p;
} SRWLOCK, *PSRWLOCK;
#define SRWLOCK_INIT {0}
#endif

/* WinSock2 event types - needed by multi.c */
#ifndef FD_MAX_EVENTS
#define FD_MAX_EVENTS 10
#endif

#ifndef _WSANETWORKEVENTS
#define _WSANETWORKEVENTS
typedef struct _WSANETWORKEVENTS {
  long lNetworkEvents;
  int iErrorCode[FD_MAX_EVENTS];
} WSANETWORKEVENTS, *PWSANETWORKEVENTS;
#endif

#ifndef WSA_INVALID_EVENT
#define WSA_INVALID_EVENT ((WSAEVENT)(WSAEVENT)-1)
#endif

#ifndef WSA_WAIT_EVENT_0
#define WSA_WAIT_EVENT_0 0
#endif

#ifndef WSA_WAIT_TIMEOUT
#define WSA_WAIT_TIMEOUT 0x102
#endif

/* stub declarations for WinSock2 event functions used by multi.c */
#ifndef _WINSOCK2_API
typedef void *WSAEVENT;
WSAEVENT WSAAPI WSACreateEvent(void);
BOOL WSAAPI WSACloseEvent(WSAEVENT hEvent);
BOOL WSAAPI WSAResetEvent(WSAEVENT hEvent);
int WSAAPI WSAEventSelect(SOCKET s, WSAEVENT hEventObject, long lNetworkEvents);
int WSAAPI WSAEnumNetworkEvents(SOCKET s, WSAEVENT hEventObject,
                                PWSANETWORKEVENTS lpNetworkEvents);
DWORD WSAAPI WSAWaitForMultipleEvents(DWORD cEvents,
                                      const WSAEVENT *lphEvents,
                                      BOOL fWaitAll, DWORD dwTimeout,
                                      BOOL fAlertable);
#endif

#endif /* _XBOX */

/* ---------------------------------------------------------------- */
/*                            TYPE SIZES                            */
/* ---------------------------------------------------------------- */

#define SIZEOF_INT 4
#define SIZEOF_LONG 4
#define SIZEOF_SIZE_T 4
#define SIZEOF_CURL_OFF_T 8
#define SIZEOF_OFF_T 4
#define SIZEOF_TIME_T 4

/* ---------------------------------------------------------------- */
/*                      LARGE FILE SUPPORT                         */
/* ---------------------------------------------------------------- */

/* ---------------------------------------------------------------- */
/*                       DNS RESOLVER SPECIALTY                     */
/* ---------------------------------------------------------------- */

/* Xbox 360 uses custom gethostbyaddr via curl */
#define USE_SYNC_DNS 1

/* ---------------------------------------------------------------- */
/*                       ADDITIONAL DEFINITIONS                     */
/* ---------------------------------------------------------------- */

/* Xbox 360 is PowerPC big-endian */
#define CURL_OS "ppc-unknown-xbox360"

/* IPv6 not supported on Xbox 360 XDK */
/* #undef USE_IPV6 */

/* Missing XDK headers */
#define CURL_NO_WIN32_CRYPTO

/* _fseeki64 available on XDK? Fallback to fseek */
/* #define HAVE_FSEEKO 1 */

/* share.h on XDK is missing _SH_DENYNO, define it */
#ifndef _SH_DENYNO
#define _SH_DENYNO 0x0040
#endif

#endif /* HEADER_CURL_CONFIG_XBOX360_H */
