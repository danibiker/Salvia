#include "quakedef.h"
#include "console.h"
#include "net.h"
#include "net_udp.h"
#include "dpmaster.h"

/* strlcpy needs the compat macro to map to strlcpy_retro__ */
#include <compat/strl.h>

/* ntohl/ntohs: <winsock2.h> provides inline implementations on MSVC.
 * On Xbox 360, <Xtl.h> (included by retro_miscellaneous.h) provides them.
 * <windows.h> is already included via quakedef.h → retro_miscellaneous.h. */
#ifdef _XBOX
#include <winsockx.h>
#elif _WIN32
#include <winsock2.h>
#include <mswsock.h>  /* SIO_UDP_CONNRESET */
#else
#include <time.h>
#endif

#define DPM_MASTER_PORT    27950
#define DPM_QUERY_TIMEOUT  8000  /* ms for master response */
#define DPM_INFO_TIMEOUT   5000  /* ms for server info responses */

static const char *dpm_master_names[] = {
	"master.frag-net.com",
	"dpmaster.deathmask.net",
	"dpmaster.tchr.no"
};
#define DPM_NUM_MASTERS ((int)(sizeof(dpm_master_names) / sizeof(dpm_master_names[0])))

/* ─── Internal state machine ─────────────────────────────── */
typedef enum {
	DPM_IDLE = 0,
	DPM_SEND_MASTER,
	DPM_WAIT_MASTER,
	DPM_SEND_INFO,
	DPM_WAIT_INFO,
	DPM_DONE,
	DPM_ERROR
} dpm_phase_t;

typedef struct {
	netadr_t addr;
	qboolean info_ok;
	char name[64];
	char map[64];
	char address[48];
	char gametype[16];
	char engine[48];
	int players;
	int max_players;
} dpm_ientry_t;

static int dpm_sock = -1;
static dpm_phase_t dpm_phase = DPM_IDLE;
static dpm_ientry_t dpm_entries[DPM_MAX_SERVERS];
static int dpm_num_entries = 0;
static int dpm_info_idx = 0;     /* next entry to query info for */
static uint32_t dpm_phase_start; /* GetTickCount when phase started (0 = unset) */
static byte dpm_buf[8192];
static uint32_t dpm_timeout;     /* ms timeout for current phase */
static qboolean dpm_started;     /* true once a query has been initiated */

/* ─── Low-level helpers ───────────────────────────────────── */
static uint32_t dpm_now_ms(void) {
#ifdef _WIN32
	return GetTickCount();
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

static qboolean dpm_timed_out(uint32_t timeout_ms) {
	uint32_t elapsed;
	if (dpm_phase_start == 0)
		return false; /* phase not started yet */
	elapsed = dpm_now_ms() - dpm_phase_start;
	if (elapsed >= timeout_ms)
		return true;
	return false;
}

/* ─── Phase: SEND_MASTER ─────────────────────────────────── */
static void dpm_send_master_queries(void) {
	/* Resolve and cache master addresses once */
	static netadr_t dpm_master_addrs[DPM_NUM_MASTERS];
	static qboolean dpm_master_resolved = false;
	int mi;

	if (!dpm_master_resolved) {
		for (mi = 0; mi < DPM_NUM_MASTERS; mi++) {
			char hostport[128];
			snprintf(hostport, sizeof(hostport), "%s:%d",
				 dpm_master_names[mi], DPM_MASTER_PORT);
			if (UDP_GetAddrFromName(hostport, &dpm_master_addrs[mi]) != 0) {
				Con_Printf("DPMaster: DNS failed for %s\n",
					   dpm_master_names[mi]);
			}
		}
		dpm_master_resolved = true;
	}

	/* Send FTE-Quake query (primary format - most servers register as this)
	 * and DarkPlaces-Quake query for legacy servers.  Protocol 3 with
	 * no trailing newline to match what the masters expect. */
	{
		static const byte q_fte[]  = {0xFF,0xFF,0xFF,0xFF,'g','e','t','s','e','r','v','e','r','s',' ','F','T','E','-','Q','u','a','k','e',' ','3',' ','e','m','p','t','y',' ','f','u','l','l',0};
		static const byte q_dp[]   = {0xFF,0xFF,0xFF,0xFF,'g','e','t','s','e','r','v','e','r','s',' ','D','a','r','k','P','l','a','c','e','s','-','Q','u','a','k','e',' ','3',' ','e','m','p','t','y',' ','f','u','l','l',0};
		static const byte *qlist[]  = {q_fte, q_dp};
		static const int qlen[]    = {sizeof(q_fte) - 1, sizeof(q_dp) - 1};
		static const int qnum       = 2;
		int qi;

		for (mi = 0; mi < DPM_NUM_MASTERS; mi++)
			for (qi = 0; qi < qnum; qi++)
				UDP_Write(dpm_sock, qlist[qi],
					  qlen[qi],
					  &dpm_master_addrs[mi]);
	}
}

/* ─── Phase: WAIT_MASTER — drain responses ───────────────── */
static void dpm_collect_master_responses(void) {
	int ret;
	netadr_t from;

	while (1) {
		const byte *p, *end;
		int skip;

		ret = UDP_Read(dpm_sock, dpm_buf, sizeof(dpm_buf), &from);
		if (ret < 0) {
			Con_Printf("DPMaster: recv error %d\n",
				   WSAGetLastError());
			break;
		}
		if (ret == 0)
			break;

		p = dpm_buf;
		end = dpm_buf + ret;
		skip = 0;

		/* eat optional 0xFF 0xFF 0xFF 0xFF header */
		if (ret >= 4 && p[0] == 0xFF && p[1] == 0xFF &&
		    p[2] == 0xFF && p[3] == 0xFF) {
			skip = 4;
		}

		/* skip "getserversResponse" (no newline, entries follow
		 * immediately as \ + 4 bytes IP + 2 bytes port each) */
		if (ret >= skip + 18 &&
		    memcmp(p + skip, "getserversResponse", 18) == 0) {
			p = p + skip + 18;
		} else {
			continue;
		}

		/* read entries: each is \ + 4 bytes IP + 2 bytes port = 7 bytes */
		while (p + 7 <= end) {
			uint32_t ip;
			uint16_t port;

			/* each entry starts with a backslash separator */
			if (*p != '\\')
				break;
			p++;

			/* check for EOT marker (\EOT\0\0\0) */
			if (p[0] == 'E' && p[1] == 'O' && p[2] == 'T' &&
			    p[3] == 0 && p[4] == 0 && p[5] == 0)
				break;

			ip   = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
			       ((uint32_t)p[2] << 8)  |  p[3];
			port = ((uint16_t)p[4] << 8) | p[5];

			if (dpm_num_entries < DPM_MAX_SERVERS) {
				dpm_ientry_t *e = &dpm_entries[dpm_num_entries];
				memset(e, 0, sizeof(*e));
				e->addr.ip.l = BigLong(ip);
				e->addr.port = BigShort((unsigned short)port);
				e->addr.pad  = 0;
				snprintf(e->address, sizeof(e->address),
					 "%d.%d.%d.%d:%d",
					 p[0], p[1], p[2], p[3], port);
				dpm_num_entries++;
			}

			p += 6;
		}
	}
}

/* ─── Send getinfo to a single server ────────────────────── */
static void dpm_send_info(int idx) {
	dpm_ientry_t *e = &dpm_entries[idx];
	static const byte info_pkt[] = {0xFF, 0xFF, 0xFF, 0xFF, 'g','e','t','i','n','f','o','\n'};
	UDP_Write(dpm_sock, info_pkt, sizeof(info_pkt), &e->addr);
}

/* ─── Parse a key/value info response ─────────────────────── */
static qboolean dpm_parse_info_response(const byte *data, int len, dpm_ientry_t *e) {
	const byte *p;

	/* skip 0xFF header if present */
	p = data;
	if (len >= 4 && p[0] == 0xFF && p[1] == 0xFF && p[2] == 0xFF && p[3] == 0xFF)
		p += 4;

	/* skip "Response\n" (DP/FTE), "infoResponse\n" (QW) or "infoResponse" */
	if (p + 9 <= data + len && memcmp(p, "Response\n", 9) == 0)
		p += 9;
	else if (p + 13 <= data + len && memcmp(p, "infoResponse\n", 13) == 0)
		p += 13;
	else if (p + 12 <= data + len && memcmp(p, "infoResponse", 12) == 0)
		p += 12;
	else
		return false;

	/* skip optional challenge (2-4 bytes) — look for first backslash */
	while (p < data + len && *p != '\\' && (unsigned char)*p > 0x20)
		p++;

	/* Parse \key\value\ pairs */
	while (p < data + len && *p == '\\') {
		char key[64], value[256];
		int kl = 0, vl = 0;

		p++; /* skip \ */
		while (p < data + len && *p != '\\' && kl < (int)sizeof(key) - 1)
			key[kl++] = *p++;
		key[kl] = 0;
		if (p < data + len && *p == '\\') p++;

		while (p < data + len && *p != '\\' && vl < (int)sizeof(value) - 1)
			value[vl++] = *p++;
		value[vl] = 0;
		/* p now points at '\' between value and next key — let while() consume it */
		if (strcasecmp(key, "hostname") == 0 || strcasecmp(key, "name") == 0) {
			strlcpy(e->name, value, sizeof(e->name));
		} else if (strcasecmp(key, "map") == 0 || strcasecmp(key, "mapname") == 0) {
			strlcpy(e->map, value, sizeof(e->map));
		} else if (strcasecmp(key, "num_players") == 0 ||
			 strcasecmp(key, "numplayers") == 0 ||
			 strcasecmp(key, "players") == 0 ||
			 strcasecmp(key, "clients") == 0)
			e->players = atoi(value);
		else if (strcasecmp(key, "max_players") == 0 ||
			 strcasecmp(key, "maxplayers") == 0 ||
			 strcasecmp(key, "maxclients") == 0 ||
			 strcasecmp(key, "sv_maxclients") == 0)
			e->max_players = atoi(value);
		else if (strcasecmp(key, "game") == 0 ||
			 strcasecmp(key, "gamename") == 0)
			strlcpy(e->gametype, value, sizeof(e->gametype));
		else if (strcasecmp(key, "*version") == 0 ||
			 strcasecmp(key, "exe") == 0)
			strlcpy(e->engine, value, sizeof(e->engine));
	}

	return true;
}

/* ─── Phase: WAIT_INFO — drain info responses ────────────── */
static void dpm_collect_info_responses(void) {
	int ret;
	netadr_t from;

	while (1) {
		int i;

		ret = UDP_Read(dpm_sock, dpm_buf, sizeof(dpm_buf), &from);
		if (ret < 0) {
			Con_Printf("DPMaster: info recv error %d\n",
				   WSAGetLastError());
			break;
		}
		if (ret == 0)
			break;

		for (i = 0; i < dpm_num_entries; i++) {
			if (dpm_entries[i].info_ok)
				continue;
			if (NET_AddrCompare(&dpm_entries[i].addr, &from) == 0) {
				if (dpm_parse_info_response(dpm_buf, ret, &dpm_entries[i]))
					dpm_entries[i].info_ok = true;
				break;
			}
		}
	}
}

/* ─── Main polling API ────────────────────────────────────── */
int DPMaster_Poll(dpm_server_t *out, int *num_out, int max_out) {
	int count = 0;

	/* Kick-off call (out == num_out == NULL): forcibly restart
	 * even if a previous query already completed. */
	if (out == NULL && num_out == NULL) {
		if (dpm_phase == DPM_DONE || dpm_phase == DPM_ERROR) {
			dpm_phase = DPM_IDLE;
			dpm_started = false;
			dpm_phase_start = 0;
		}
	}

	/* Persisted DONE — no new data to report */
	if (dpm_phase == DPM_DONE && dpm_sock == -1) {
		if (num_out) *num_out = 0;
		return 1;
	}

	if (!dpm_started) {
		dpm_started = true;
		dpm_phase = DPM_SEND_MASTER;
		dpm_num_entries = 0;
		dpm_info_idx = 0;
	}

	if (dpm_phase == DPM_ERROR) {
		if (num_out) *num_out = 0;
		return -1;
	}

	switch (dpm_phase) {

	case DPM_SEND_MASTER:
		if (dpm_sock == -1) {
			dpm_sock = UDP_OpenSocket(0);
			if (dpm_sock == -1) {
				dpm_phase = DPM_ERROR;
				Con_Printf("DPMaster: failed to open UDP socket\n");
				return -1;
			}
			/* Disable WSAECONNRESET on UDP sockets (Windows):
			 * prevents ICMP unreachable from poisoning the
			 * socket and causing recvfrom to return
			 * WSAECONNRESET instead of WSAEWOULDBLOCK. */
#ifndef _XBOX360
			{
				DWORD bytes = 0;
				BOOL off = FALSE;
				WSAIoctl(dpm_sock, SIO_UDP_CONNRESET,
					 &off, sizeof(off),
					 NULL, 0, &bytes, NULL, NULL);
			}
#endif
		}
		dpm_send_master_queries();
		dpm_phase_start = dpm_now_ms();
		dpm_timeout = DPM_QUERY_TIMEOUT;
		dpm_phase = DPM_WAIT_MASTER;
		return 0;

	case DPM_WAIT_MASTER:
		dpm_collect_master_responses();
		if (dpm_timed_out(dpm_timeout)) {
			dpm_info_idx = 0;
			dpm_phase = DPM_SEND_INFO;
		}
		return 0;

	case DPM_SEND_INFO:
		/* Send info queries in small batches per frame */
		{
			int i;
			for (i = 0; i < 8 && dpm_info_idx < dpm_num_entries; i++)
				dpm_send_info(dpm_info_idx++);
			if (dpm_info_idx >= dpm_num_entries) {
				dpm_phase_start = dpm_now_ms();
				dpm_timeout = DPM_INFO_TIMEOUT;
				dpm_phase = DPM_WAIT_INFO;
			}
		}
		return 0;

	case DPM_WAIT_INFO:
		dpm_collect_info_responses();
		if (dpm_timed_out(dpm_timeout))
			dpm_phase = DPM_DONE;
		return 0;

	case DPM_DONE:
		; /* fall through to done */
	}

done:
	if (out && num_out && max_out > 0) {
		int i;
		for (i = 0; i < dpm_num_entries && count < max_out; i++) {
			dpm_ientry_t *in = &dpm_entries[i];
			dpm_server_t *o  = &out[count];

			if (!in->info_ok)
				continue;

			/* skip engines known to use incompatible protocols */
			if (strncasecmp(in->engine, "QSS", 3) == 0 ||
			    strncasecmp(in->engine, "ezQuake", 7) == 0)
				continue;

			strlcpy(o->name, in->name, sizeof(o->name));
			strlcpy(o->map,  in->map,  sizeof(o->map));
			strlcpy(o->address,  in->address,  sizeof(o->address));
			strlcpy(o->gametype, in->gametype, sizeof(o->gametype));
			o->players    = in->players;
			o->max_players = in->max_players;
			count++;
		}
	}

	/* Cleanup — persist DPM_DONE so we don't auto-restart next frame */
	if (dpm_sock != -1) {
		UDP_CloseSocket(dpm_sock);
		dpm_sock = -1;
	}
	dpm_phase = DPM_DONE;          /* persist completed state */
	/* keep dpm_started = true */  /* prevents auto-restart */
	dpm_num_entries = 0;
	if (num_out) *num_out = count;
	return 1;
}

void DPMaster_Cancel(void) {
	if (dpm_sock != -1) {
		UDP_CloseSocket(dpm_sock);
		dpm_sock = -1;
	}
	dpm_phase = DPM_IDLE;
	dpm_started = false;
	dpm_num_entries = 0;
	dpm_phase_start = 0;
}
