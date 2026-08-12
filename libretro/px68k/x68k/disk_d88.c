#include "common.h"
#include "../libretro/dosio.h"
#include "fdc.h"
#include "fdd.h"
#include "disk_d88.h"

/* セクター部 (16 Bytes) */
typedef struct {
	uint8_t	c;
	uint8_t	h;
	uint8_t	r;
	uint8_t	n;
	uint16_t	sectors;		      /* Sector Count */
	uint8_t	mfm_flg;		      /* sides */
	uint8_t	del_flg;		      /* DELETED DATA */
	uint8_t	stat;			      /* STATUS (FDC ret) */
	uint8_t	reserved2[5];		/* Reserved */
	uint16_t	size;			      /* Sector Size */
} D88_SECTOR;

typedef struct D88_SECTINFO {
	struct D88_SECTINFO* next;
	D88_SECTOR sect;
} D88_SECTINFO;

static D88_HEADER    D88Head[4];
static D88_SECTINFO* D88Trks[4][164];
static char          D88File[4][MAX_PATH];
static D88_SECTINFO* D88Cur[4] = {0, 0, 0, 0};
static D88_SECTINFO* D88Top[4] = {0, 0, 0, 0};

void D88_Init(void)
{
	int drv, trk;

	for (drv=0; drv<4; drv++) {
		for (trk=0; trk<164; trk++) D88Trks[drv][trk] = 0;
		memset(&D88Head[drv], 0, sizeof(D88_HEADER));
		memset(D88File[drv], 0, MAX_PATH);
	}
}


void D88_Cleanup(void)
{
	int drv;
	for (drv=0; drv<4; drv++) D88_Eject(drv);
}


int D88_SetFD(int drv, char* filename)
{
	int trk, sct;
	void *fp;
	D88_SECTOR d88s;

	strncpy(D88File[drv], filename, MAX_PATH);
	D88File[drv][MAX_PATH-1] = 0;

	if (!(fp = file_open(D88File[drv])))
   {
      memset(D88File[drv], 0, MAX_PATH);
      return 0;
   }
	file_seek(fp, 0, FSEEK_SET);
	if ( file_lread(fp, &D88Head[drv], sizeof(D88_HEADER))!=sizeof(D88_HEADER) ) goto d88_set_error;

	if ( D88Head[drv].protect )
		FDD_SetReadOnly(drv);

	for (trk=0; trk<164; trk++)
   {
	   /* TODO/FIXME - replace long type */
      long ptr = D88Head[drv].trackp[trk];
      D88_SECTINFO *si, *oldsi;

      if ( (ptr>=(long)sizeof(D88_HEADER))&&(ptr<D88Head[drv].fd_size) ) {
         d88s.sectors = 65535;
         file_seek(fp, (size_t)ptr, FSEEK_SET);
         for (sct=0; sct<d88s.sectors; sct++) {
            if ( file_lread(fp, &d88s, sizeof(D88_SECTOR))!=sizeof(D88_SECTOR) ) goto d88_set_error;
            si = (D88_SECTINFO*)malloc(sizeof(D88_SECTINFO)+d88s.size);
            if ( !si ) goto d88_set_error;
            if ( sct ) {
               if (oldsi) oldsi->next = si;
            } else {
               D88Trks[drv][trk] = si;
            }
            memcpy(&si->sect, &d88s, sizeof(D88_SECTOR));
            if ( file_lread(fp, ((uint8_t*)si)+sizeof(D88_SECTINFO), d88s.size)!=d88s.size ) goto d88_set_error;
            si->next = 0;
            oldsi    = si;
         }
      }
   }
	file_close(fp);
	return 1;

d88_set_error:
	file_close(fp);
	FDD_SetReadOnly(drv);
	return 0;
}


int D88_Eject(int drv)
{
	int trk, pos;
	void *fp;

	if ( !D88File[drv][0] ) return 0;

	if ( !FDD_IsReadOnly(drv) )
   {
      if ((fp = file_open(D88File[drv])))
      {
         pos = sizeof(D88_HEADER);
         for (trk=0; trk<164; trk++)
         {
            D88_SECTINFO *si = D88Trks[drv][trk];
            if ( si )
               D88Head[drv].trackp[trk] = pos;
            else
               D88Head[drv].trackp[trk] = 0;
            while ( si ) {
               pos += (sizeof(D88_SECTOR)+si->sect.size);
               si = si->next;
            }
         }
         D88Head[drv].fd_size = pos;
         file_lwrite(fp, &D88Head[drv], sizeof(D88_HEADER));
         for (trk=0; trk<164; trk++) {
            D88_SECTINFO *si = D88Trks[drv][trk];
            while ( si ) {
               file_lwrite(fp, &si->sect, sizeof(D88_SECTOR)+si->sect.size);
               si = si->next;
            }
            D88Trks[drv][trk] = 0;
         }
         file_close(fp);
      }
   }

	for (trk=0; trk<164; trk++)
   {
      D88_SECTINFO *si = D88Trks[drv][trk];
      while ( si ) {
         D88_SECTINFO *nextsi = si->next;
         free(si);
         si = nextsi;
      }
      D88Trks[drv][trk] = 0;
   }
	memset(&D88Head[drv], 0, sizeof(D88_HEADER));
	memset(D88File[drv], 0, MAX_PATH);

	return 1;
}

int D88_Seek(int drv, int trk, FDCID* id)
{
	if ( (drv<0)||(drv>3) ) return 0;
	if ( (trk<0)||(trk>163) ) return 0;
	if ( !D88Trks[drv][trk] ) return 0;
	if ( D88Top[drv]!=D88Trks[drv][trk] ) {
		D88Cur[drv] = D88Top[drv] = D88Trks[drv][trk];
	}
	id->c = D88Cur[drv]->sect.c;
	id->h = D88Cur[drv]->sect.h;
	id->r = D88Cur[drv]->sect.r;
	id->n = D88Cur[drv]->sect.n;
	return 1; 
}


int D88_GetCurrentID(int drv, FDCID* id)
{
	if ( !D88Cur[drv] ) return 0;
	id->c = D88Cur[drv]->sect.c;
	id->h = D88Cur[drv]->sect.h;
	id->r = D88Cur[drv]->sect.r;
	id->n = D88Cur[drv]->sect.n;
	return 1;
}


int D88_ReadID(int drv, FDCID* id)
{
	D88_SECTINFO *si = D88Cur[drv];
	int ret = 1;
	if ( !si ) return 0;
	id->c = si->sect.c;
	id->h = si->sect.h;
	id->r = si->sect.r;
	id->n = si->sect.n;
	if ( si->next )
		D88Cur[drv] = si->next;
	else
		D88Cur[drv] = D88Top[drv];
	if ( si->sect.del_flg&0x10 ) ret |= 2;
	if ( si->sect.stat==0xa0 )   ret |= 4;
	if ( si->sect.stat==0xb0 )   ret |= 8;
	if ( si->sect.stat==0xf0 )   ret |= 16;
	return ret;
}


int D88_WriteID(int drv, int trk, uint8_t* buf, int num)
{
	int i;
	uint8_t c = buf[num<<2];
	if ( (drv<0)||(drv>3) ) return 0;
	if ( (trk<0)||(trk>163) ) return 0;
	if ( D88Trks[drv][trk] ) {
		D88_SECTINFO *si = D88Trks[drv][trk];
		while ( si ) {
			D88_SECTINFO *nextsi = si->next;
			free(si);
			si = nextsi;
		}
		D88Trks[drv][trk] = 0;
	}
	for (i=0; i<num; i++, buf+=4) {
		int size = 128<<buf[3];
		D88_SECTINFO *si = (D88_SECTINFO*)malloc(sizeof(D88_SECTINFO)+size), *oldsi = NULL;
		if ( !si ) goto d88_writeid_error;
		if ( i ) {
			if (oldsi) oldsi->next = si;
		} else {
			D88Trks[drv][trk] = si;
		}
		memset(&si->sect, 0, sizeof(D88_SECTOR));
		si->sect.c = buf[0];
		si->sect.h = buf[1];
		si->sect.r = buf[2];
		si->sect.n = buf[3];
		si->sect.sectors = num;
		si->sect.size = size;
		memset(((uint8_t*)si)+sizeof(D88_SECTINFO), c, size);
		si->next = 0;
		oldsi = si;
	}
	D88Cur[drv] = D88Top[drv] = D88Trks[drv][trk];
	return 1;

d88_writeid_error:
	return 0;
}


int D88_Read(int drv, FDCID* id, uint8_t* buf)
{
	D88_SECTINFO *si = D88Top[drv];
	if ( !si ) return 0;
	do {
		if ( (id->c==si->sect.c)&&(id->h==si->sect.h)&&(id->r==si->sect.r)&&(id->n==si->sect.n) ) {
			int len = 128<<id->n;
			int ret = 1;
			memcpy(buf, ((uint8_t*)si)+sizeof(D88_SECTINFO), len);
			if ( si->next )
				D88Cur[drv] = si->next;
			else
				D88Cur[drv] = D88Top[drv];
			if ( si->sect.del_flg&0x10 ) ret |= 2;
			if ( si->sect.stat==0xa0 )   ret |= 4;
			if ( si->sect.stat==0xb0 )   ret |= 8;
			if ( si->sect.stat==0xf0 )   ret |= 16;
			return ret;
		}
		si = si->next;
	} while ( si );
	D88Cur[drv] = D88Top[drv];
	return 0;
}


int D88_ReadDiag(int drv, FDCID* id, FDCID* retid, uint8_t* buf)
{
	D88_SECTINFO *si = D88Cur[drv];
	int size = 128<<id->n;
	int ret = 1;
	if ( !si ) return 0;
	memcpy(buf, ((uint8_t*)si)+sizeof(D88_SECTINFO), size);
	if ( si->next )
		D88Cur[drv] = si->next;
	else
		D88Cur[drv] = D88Top[drv];
	retid->c = si->sect.c;
	retid->r = si->sect.r;
	retid->h = si->sect.h;
	retid->n = si->sect.n;
	if ( si->sect.del_flg&0x10 ) ret |= 2;
	if ( si->sect.stat==0xa0 )   ret |= 4;
	if ( si->sect.stat==0xb0 )   ret |= 8;
	if ( si->sect.stat==0xf0 )   ret |= 16;
	return ret;
}


int D88_Write(int drv, FDCID* id, uint8_t* buf, int del)
{
	D88_SECTINFO *si = D88Top[drv];
	if ( !si ) return 0;
	if ( FDD_IsReadOnly(drv) ) return 0;
	do {
		if ( (id->c==si->sect.c)&&(id->h==si->sect.h)&&(id->r==si->sect.r)&&(id->n==si->sect.n) ) {
			int len = 128<<id->n;
			memcpy(((uint8_t*)si)+sizeof(D88_SECTINFO), buf, len);
			si->sect.del_flg = ((del)?0x10:0x00);
			if ( si->next )
				D88Cur[drv] = si->next;
			else
				D88Cur[drv] = D88Top[drv];
			return 1;
		}
		si = si->next;
	} while ( si );
	D88Cur[drv] = D88Top[drv];
	return 0;
}
