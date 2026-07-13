/*
	3DOplay sources v1.7.3 based on FreeDOcore
	3doplay.do.am
	Developer: Viktor Ivanov
	Any uses of the 3DOplay sources or any other material published by Viktor Ivanov have to be accompanied with full credits.
All rights reserved.
*/
/*
  www.freedo.org
The first and only working 3DO multiplayer emulator.

The FreeDO licensed under modified GNU LGPL, with following notes:

*   The owners and original authors of the FreeDO have full right to develop closed source derivative work.
*   Any non-commercial uses of the FreeDO sources or any knowledge obtained by studying or reverse engineering
    of the sources, or any other material published by FreeDO have to be accompanied with full credits.
*   Any commercial uses of FreeDO sources or any knowledge obtained by studying or reverse engineering of the sources,
    or any other material published by FreeDO is strictly forbidden without owners approval.

The above notes are taking precedence over GNU LGPL in conflicting situations.

Project authors:

Alexander Troosh
Maxim Grishin
Allen Wright
John Sammons
Felix Lazarev
*/

#include "3doplay.h"
#include "VDLP.h"
#include "arm.h"
#include <memory.h>

#include "3doplay.h"

extern _ext_Interface  io_interface;

/* === VDL Palette data === */
#define VDL_CONTROL     0x80000000
#define VDL_BACKGROUND	0xE0000000
#define VDL_RGBCTL_MASK 0x60000000
#define VDL_PEN_MASK    0x1F000000
#define VDL_R_MASK      0x00FF0000
#define VDL_G_MASK      0x0000FF00
#define VDL_B_MASK      0x000000FF

#define VDL_B_SHIFT       0
#define VDL_G_SHIFT       8
#define VDL_R_SHIFT       16
#define VDL_PEN_SHIFT     24
#define VDL_RGBSEL_SHIFT  29

/* VDL_RGBCTL_MASK definitions */
#define VDL_FULLRGB     0x00000000
#define VDL_REDONLY     0x60000000
#define VDL_GREENONLY   0x40000000
#define VDL_BLUEONLY    0x20000000


struct VDLDatum
{
        unsigned char CLUTB[32];
        unsigned char CLUTG[32];
        unsigned char CLUTR[32];
        unsigned int BACKGROUND;
        unsigned int HEADVDL;
        unsigned int MODULO;
        unsigned int CURRENTVDL;
        unsigned int CURRENTBMP;
        unsigned int PREVIOUSBMP;
        unsigned int OUTCONTROLL;
        int linedelay;
};

static VDLDatum vdl;
static unsigned char * vram;
static unsigned int CLUTraw;

unsigned int _vdl_SaveSize()
{
        return sizeof(VDLDatum);
}
void _vdl_Save(void *buff)
{
        memcpy(buff,&vdl,sizeof(VDLDatum));
}
void _vdl_Load(void *buff)
{
        memcpy(&vdl,buff,sizeof(VDLDatum));
}

#define CLUTB vdl.CLUTB
#define CLUTG vdl.CLUTG
#define CLUTR vdl.CLUTR
#define BACKGROUND vdl.BACKGROUND
#define HEADVDL vdl.HEADVDL
#define MODULO vdl.MODULO
#define CURRENTVDL vdl.CURRENTVDL
#define CURRENTBMP vdl.CURRENTBMP
#define PREVIOUSBMP vdl.PREVIOUSBMP
#define OUTCONTROLL vdl.OUTCONTROLL
#define CLUTDMA vdl.CLUTDMA
#define linedelay vdl.linedelay


unsigned int vmreadw(unsigned int addr);
//static AString str;

void _vdl_ProcessVDL( unsigned int addr)
{
	HEADVDL=addr;

	if((addr&0xfff00000)!=0x200000)
	{
		//io_interface(EXT_DEBUG_PRINT,(void*)str.print("!!!!VDLP!!!! Код за границами VRAM!!! 0x%8.8X",addr).CStr());
	}

}

static const unsigned int HOWMAYPIXELEXPECTPERLINE[8] =
	{320, 384, 512, 640, 1024, 320, 320, 320};

// ###### Per line implementation ######

int doloadclut=0;
__inline void VDLExec()
{
 unsigned int NEXTVDL,tmp;
 unsigned char ifgnorflag=0;


		tmp=vmreadw(CURRENTVDL);
		if(tmp==0) // End of list
		{
			linedelay=511;
            doloadclut=1;
		}
		else
		{
			CLUTraw=tmp;

			if((tmp & 0x10000) >> 16)
				CURRENTBMP=vmreadw(CURRENTVDL+4);
			if((tmp & 0x8000) >> 15)
				PREVIOUSBMP=vmreadw(CURRENTVDL+8);
			if((tmp & 0x20000) >> 18)
			{
				NEXTVDL=(CURRENTVDL+vmreadw(CURRENTVDL+12)+16);
				//CDebug::DPrint("Relative offset??\n");
			}
			else
				NEXTVDL=vmreadw(CURRENTVDL+12);

			CURRENTVDL+=16;

			int nmcmd=((tmp & 0x7E00) >> 9);	//nmcmd-=4;?
			for(int i=0; i<nmcmd; i++)
			{
                   
				int cmd=vmreadw(CURRENTVDL);
				 //if(CURRENTVDL>cmd)
                 // CURRENTVDL-=4;
                 CURRENTVDL+=4;

				    if(!(cmd&VDL_CONTROL))
					{	//color value

						unsigned int coloridx=(cmd&VDL_PEN_MASK)>>VDL_PEN_SHIFT;
						if((cmd&VDL_RGBCTL_MASK)==VDL_FULLRGB)
						{
                                                            
							CLUTR[coloridx]=(cmd&VDL_R_MASK)>>VDL_R_SHIFT;
							CLUTG[coloridx]=(cmd&VDL_G_MASK)>>VDL_G_SHIFT;
							CLUTB[coloridx]=(cmd&VDL_B_MASK)>>VDL_B_SHIFT;
						}
						else if(cmd&VDL_RGBCTL_MASK==VDL_REDONLY)
							CLUTR[coloridx]=(cmd&VDL_R_MASK)>>VDL_R_SHIFT;
						else if(cmd&VDL_RGBCTL_MASK==VDL_GREENONLY)
							CLUTG[coloridx]=(cmd&VDL_G_MASK)>>VDL_G_SHIFT;
						else if(cmd&VDL_RGBCTL_MASK==VDL_BLUEONLY)
							CLUTB[coloridx]=(cmd&VDL_B_MASK)>>VDL_B_SHIFT;
					}
					else if((cmd&0xff000000)==VDL_BACKGROUND)
					{
                                                if(ifgnorflag)continue;
                			        BACKGROUND= ((     cmd&0xFF    )<<16)|
													(( cmd&0xFF00 )) |
											        (((cmd>>16)&0xFF) );
                	                }
					else if((cmd&0xE0000000)==0xc0000000)
					{
							if(ifgnorflag)continue;
							OUTCONTROLL=cmd;
                            ifgnorflag=OUTCONTROLL&2;
							//if(!ifgnorflag)break;
					}
					else if(cmd==0xffffffff)
					{
						if(ifgnorflag)continue;
						for(unsigned int j=0;j<32;j++)
						{
							CLUTB[j]=CLUTG[j]=CLUTR[j]=((j&0x1f)<<3)|((j>>2)&7);
						}
					}

			}//for(i<nmcmd)
			CURRENTVDL=NEXTVDL;

			MODULO=HOWMAYPIXELEXPECTPERLINE[(tmp & 0x1C00000) >> 22];
 
			doloadclut=linedelay=(tmp & 0x1FF);
		}
}


__inline uint32 VRAMOffEval(uint32 addr, uint32 line)
{
        return ((((~addr)&2)<<18)+((addr>>2)<<1)+1024*512*line);
}

void _vdl_DoLineNew(int line2x, VDLFrame *frame)
{
  int y,i;
  int line=line2x&0x7ff;


	if(line==0)
	{
        doloadclut=1;
        linedelay=0;
		CURRENTVDL=HEADVDL;
		VDLExec();
	}

	y=(line-(16));

	if(linedelay==0)
	{
	    VDLExec();
	}

	if((y>=0) && (y<256))  // 256???
	{

                if((CLUTraw & 0x10000) >> 15)
                {
                        {                       
                                unsigned short *dst;
                                unsigned int *src;
                                dst=frame->lines[y].line;
                                src=(unsigned int*)(vram+((PREVIOUSBMP) & 0x0FFFFF));
                                i=320;
								__dcbt(0,&src);
								__dcbt(4,&src+1);
								__dcbt(8,&src+2);
								__dcbt(12,&src+3);
								__dcbt(16,&src+4);
								__dcbt(20,&src+5);
								__dcbt(24,&src+6);
								__dcbt(28,&src+7);
								__lwsync();
                                while(i--)*dst++=*(unsigned short*)(src++);
                        }
                        XMemCpy(frame->lines[y].xCLUTB,CLUTB,32);
                        XMemCpy(frame->lines[y].xCLUTG,CLUTG,32);
                        XMemCpy(frame->lines[y].xCLUTR,CLUTR,32);
                }
                frame->lines[y].xOUTCONTROLL=OUTCONTROLL;
                frame->lines[y].xCLUTDMA=CLUTraw;
                frame->lines[y].xBACKGROUND=BACKGROUND;

	} // //if((y>=0) && (y<240))

        if(CURRENTBMP & 2)
		CURRENTBMP+=MODULO*4 - 2;
	else
		CURRENTBMP+=2;

	if(!(CLUTraw & 0x4000) >> 14)
	{
		PREVIOUSBMP=CURRENTBMP;
	}
	else
	{
		if(PREVIOUSBMP & 2)
			PREVIOUSBMP+=MODULO*4 - 2;
		else
			PREVIOUSBMP+=2;
	}


	linedelay--;
	OUTCONTROLL&=~1; //Vioff1ln
}


void _vdl_Init(unsigned char *vramstart)
{
	vram=vramstart;

	static const unsigned int StartupVDL[]=
	{ // Startup VDL at addres 0x2B0000
		0x00004410, 0x002C0000, 0x002C0000, 0x002B0098,
		0x00000000, 0x01080808, 0x02101010, 0x03191919,
		0x04212121, 0x05292929, 0x06313131, 0x073A3A3A,
		0x08424242, 0x094A4A4A, 0x0A525252, 0x0B5A5A5A,
		0x0C636363, 0x0D6B6B6B, 0x0E737373, 0x0F7B7B7B,
		0x10848484, 0x118C8C8C, 0x12949494, 0x139C9C9C,
		0x14A5A5A5, 0x15ADADAD, 0x16B5B5B5, 0x17BDBDBD,
		0x18C5C5C5, 0x19CECECE, 0x1AD6D6D6, 0x1BDEDEDE,
		0x1CE6E6E6, 0x1DEFEFEF, 0x1EF8F8F8, 0x1FFFFFFF,
		0xE0010101, 0xC001002C, 0x002180EF, 0x002C0000,
		0x002C0000, 0x002B00A8, 0x00000000, 0x002C0000,
		0x002C0000, 0x002B0000
	};
	HEADVDL=0xB0000;

        for(int i=0;i<(sizeof(StartupVDL)/4);i++)
                _mem_write32(HEADVDL+i*4+1024*1024*2,StartupVDL[i]);

	//memcpy(vram+HEADVDL, StartupVDL, sizeof(StartupVDL));


	for(unsigned int i=0;i<32;i++)
	{
		CLUTB[i]=CLUTG[i]=CLUTR[i]=((i&0x1f)<<3)|((i>>2)&7);
	}
}

unsigned int vmreadw(unsigned int addr)
{

	return _mem_read32((addr&0xfffff)+1024*1024*2);

}
