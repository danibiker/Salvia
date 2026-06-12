#ifndef DPMATER_H
#define DPMATER_H

#include "net.h"

#define DPM_MAX_SERVERS 256

typedef struct {
	char name[64];
	char map[64];
	char address[48];
	char gametype[16];
	int players;
	int max_players;
} dpm_server_t;

/* Poll-based async dpmaster query.  Call once per frame.
 * Returns 1 when done (results in servers/num_servers),
 * 0 while in progress, -1 on error. */
int DPMaster_Poll(dpm_server_t *servers, int *num_servers, int max_servers);

void DPMaster_Cancel(void);

#endif
