/* xbox360_compat.c - Xbox 360 compatibility stubs for curl */
#include "curl_setup.h"

#ifdef _XBOX

#include <stdlib.h>
#include <string.h>

/* ===================================================================
 * Missing WinSock2 type stubs
 * =================================================================== */

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

/* WSAIoctl stub - not available on Xbox 360 */
int WSAAPI WSAIoctl(SOCKET s, unsigned long dwIoControlCode,
                    void *lpvInBuffer, unsigned long cbInBuffer,
                    void *lpvOutBuffer, unsigned long cbOutBuffer,
                    unsigned long *lpcbBytesReturned,
                    void *lpOverlapped, void *lpCompletionRoutine)
{
  (void)s; (void)dwIoControlCode; (void)lpvInBuffer; (void)cbInBuffer;
  (void)lpvOutBuffer; (void)cbOutBuffer; (void)lpcbBytesReturned;
  (void)lpOverlapped; (void)lpCompletionRoutine;
  return -1; /* SOCKET_ERROR */
}

/* gethostname stub - returns Xbox name or empty string */
int WSAAPI gethostname(char *name, int namelen)
{
  if(name && namelen > 0) {
    name[0] = '\0';
  }
  return 0;
}

/* ===================================================================
 * DNS resolution via XNet (Xbox 360 networking)
 * =================================================================== */

/* Synchronous DNS lookup via XNetDnsLookup + XWSACreateEvent polling */
static int xbox360_dns_lookup(const char *hostname, IN_ADDR *out_addr)
{
  XNDNS *pxndns = NULL;
  WSAEVENT hEvent;
  int ret;

  if(!hostname || !out_addr)
    return -1;

  hEvent = WSACreateEvent();
  if(hEvent == WSA_INVALID_EVENT)
    return -1;

  ret = XNetDnsLookup(hostname, hEvent, &pxndns);
  if(ret != 0) {
    WSACloseEvent(hEvent);
    return -1;
  }

  /* Poll until DNS resolution completes */
  while(pxndns->iStatus == WSAEINPROGRESS) {
    WSAWaitForMultipleEvents(1, &hEvent, FALSE, 100, FALSE);
  }

  WSACloseEvent(hEvent);

  if(pxndns->iStatus != 0 || pxndns->cina == 0) {
    XNetDnsRelease(pxndns);
    return -1;
  }

  *out_addr = pxndns->aina[0];
  XNetDnsRelease(pxndns);
  return 0;
}

/* ===================================================================
 * getaddrinfo implementation (uses XNet DNS)
 * =================================================================== */

int getaddrinfo(const char *nodename, const char *servname,
                const struct addrinfo *hints, struct addrinfo **res)
{
  struct addrinfo *ai;
  struct sockaddr_in *sin;
  IN_ADDR addr;

  if(!nodename || !res)
    return EAI_MEMORY;

  *res = NULL;

  if(xbox360_dns_lookup(nodename, &addr) != 0)
    return EAI_NODATA;

  ai = (struct addrinfo *)malloc(sizeof(struct addrinfo));
  if(!ai)
    return EAI_MEMORY;
  memset(ai, 0, sizeof(*ai));

  sin = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
  if(!sin) {
    free(ai);
    return EAI_MEMORY;
  }
  memset(sin, 0, sizeof(*sin));
  sin->sin_family = AF_INET;
  sin->sin_addr = addr;
  if(servname)
    sin->sin_port = htons((unsigned short)atoi(servname));

  ai->ai_family = AF_INET;
  ai->ai_socktype = hints ? hints->ai_socktype : SOCK_STREAM;
  ai->ai_protocol = hints ? hints->ai_protocol : IPPROTO_TCP;
  ai->ai_addrlen = sizeof(struct sockaddr_in);
  ai->ai_addr = (struct sockaddr *)sin;
  ai->ai_canonname = NULL;
  ai->ai_next = NULL;

  *res = ai;
  return 0;
}

void freeaddrinfo(struct addrinfo *res)
{
  struct addrinfo *next;
  while(res) {
    next = res->ai_next;
    if(res->ai_addr)
      free(res->ai_addr);
    if(res->ai_canonname)
      free(res->ai_canonname);
    free(res);
    res = next;
  }
}

/* ===================================================================
 * gethostbyname (minimal implementation, used by Curl_he2ai fallback)
 * =================================================================== */

static struct hostent s_he;
static char s_he_name[256];
static char *s_he_aliases[1] = { NULL };
static IN_ADDR s_he_addr;
static char *s_he_addr_list[2];

struct hostent *gethostbyname(const char *name)
{
  IN_ADDR addr;

  if(!name)
    return NULL;

  if(xbox360_dns_lookup(name, &addr) != 0)
    return NULL;

  s_he.h_name = s_he_name;
  strncpy(s_he_name, name, sizeof(s_he_name) - 1);
  s_he_name[sizeof(s_he_name) - 1] = '\0';

  s_he.h_aliases = s_he_aliases;
  s_he.h_addrtype = AF_INET;
  s_he.h_length = sizeof(IN_ADDR);

  s_he_addr = addr;
  s_he_addr_list[0] = (char *)&s_he_addr;
  s_he_addr_list[1] = NULL;
  s_he.h_addr_list = s_he_addr_list;

  return &s_he;
}

#endif /* _XBOX */
