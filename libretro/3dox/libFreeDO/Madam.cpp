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
#include "Madam.h"
#include "Clio.h"
#include "vdlp.h"
#include "arm.h"
#include <math.h>
#include <memory.h>
#include <xtl.h>
#include <omp.h>
#include <stdio.h>
#include "bitop.h"
//BitReaderBig bitoper;

#include "3doplay.h"

extern _ext_Interface  io_interface;

/* === CCB control word flags === */
#define CCB_SKIP        0x80000000
#define CCB_LAST        0x40000000
#define CCB_NPABS       0x20000000
#define CCB_SPABS       0x10000000
#define CCB_PPABS       0x08000000
#define CCB_LDSIZE      0x04000000
#define CCB_LDPRS       0x02000000
#define CCB_LDPPMP      0x01000000
#define CCB_LDPLUT      0x00800000
#define CCB_CCBPRE      0x00400000
#define CCB_YOXY        0x00200000
#define CCB_ACSC        0x00100000
#define CCB_ALSC        0x00080000
#define CCB_ACW         0x00040000
#define CCB_ACCW        0x00020000
#define CCB_TWD         0x00010000
#define CCB_LCE         0x00008000
#define CCB_ACE         0x00004000
#define CCB_reserved13  0x00002000
#define CCB_MARIA       0x00001000
#define CCB_PXOR        0x00000800
#define CCB_USEAV       0x00000400
#define CCB_PACKED      0x00000200
#define CCB_POVER_MASK  0x00000180
#define CCB_PLUTPOS     0x00000040
#define CCB_BGND        0x00000020
#define CCB_NOBLK       0x00000010
#define CCB_PLUTA_MASK  0x0000000F

#define CCB_POVER_SHIFT  7
#define CCB_PLUTA_SHIFT  0

#define PMODE_PDC   ((0x00000000)<<CCB_POVER_SHIFT) /* Normal */
#define PMODE_ZERO  ((0x00000002)<<CCB_POVER_SHIFT)
#define PMODE_ONE   ((0x00000003)<<CCB_POVER_SHIFT)

/* === Cel first preamble word flags === */
#define PRE0_LITERAL    0x80000000
#define PRE0_BGND       0x40000000
#define PREO_reservedA  0x30000000
#define PRE0_SKIPX_MASK 0x0F000000
#define PREO_reservedB  0x00FF0000
#define PRE0_VCNT_MASK  0x0000FFC0
#define PREO_reservedC  0x00000020
#define PRE0_LINEAR     0x00000010
#define PRE0_REP8       0x00000008
#define PRE0_BPP_MASK   0x00000007

#define PRE0_SKIPX_SHIFT 24
#define PRE0_VCNT_SHIFT  6
#define PRE0_BPP_SHIFT   0

/* PRE0_BPP_MASK definitions */
#define PRE0_BPP_1   0x00000001
#define PRE0_BPP_2   0x00000002
#define PRE0_BPP_4   0x00000003
#define PRE0_BPP_6   0x00000004
#define PRE0_BPP_8   0x00000005
#define PRE0_BPP_16  0x00000006

/* Subtract this value from the actual vertical source line count */
#define PRE0_VCNT_PREFETCH    1


/* === Cel second preamble word flags === */
#define PRE1_WOFFSET8_MASK   0xFF000000
#define PRE1_WOFFSET10_MASK  0x03FF0000
#define PRE1_NOSWAP          0x00004000
#define PRE1_TLLSB_MASK      0x00003000
#define PRE1_LRFORM          0x00000800
#define PRE1_TLHPCNT_MASK    0x000007FF

#define PRE1_WOFFSET8_SHIFT   24
#define PRE1_WOFFSET10_SHIFT  16
#define PRE1_TLLSB_SHIFT      12
#define PRE1_TLHPCNT_SHIFT    0

#define PRE1_TLLSB_0     0x00000000
#define PRE1_TLLSB_PDC0  0x00001000 /* Normal */
#define PRE1_TLLSB_PDC4  0x00002000
#define PRE1_TLLSB_PDC5  0x00003000

/* Subtract this value from the actual word offset */
#define PRE1_WOFFSET_PREFETCH 2
/* Subtract this value from the actual pixel count */
#define PRE1_TLHPCNT_PREFETCH 1

#define PPMP_0_SHIFT 0
#define PPMP_1_SHIFT 16

#define PPMPC_1S_MASK  0x00008000
#define PPMPC_MS_MASK  0x00006000
#define PPMPC_MF_MASK  0x00001C00
#define PPMPC_SF_MASK  0x00000300
#define PPMPC_2S_MASK  0x000000C0
#define PPMPC_AV_MASK  0x0000003E
#define PPMPC_2D_MASK  0x00000001

#define PPMPC_MS_SHIFT  13
#define PPMPC_MF_SHIFT  10
#define PPMPC_SF_SHIFT  8
#define PPMPC_2S_SHIFT  6
#define PPMPC_AV_SHIFT  1

/* PPMPC_1S_MASK definitions */
#define PPMPC_1S_PDC   0x00000000
#define PPMPC_1S_CFBD  0x00008000

/* PPMPC_MS_MASK definitions */
#define PPMPC_MS_CCB         0x00000000
#define PPMPC_MS_PIN         0x00002000
#define PPMPC_MS_PDC_MFONLY  0x00004000
#define PPMPC_MS_PDC         0x00004000

/* PPMPC_MF_MASK definitions */
#define PPMPC_MF_1  0x00000000
#define PPMPC_MF_2  0x00000400
#define PPMPC_MF_3  0x00000800
#define PPMPC_MF_4  0x00000C00
#define PPMPC_MF_5  0x00001000
#define PPMPC_MF_6  0x00001400
#define PPMPC_MF_7  0x00001800
#define PPMPC_MF_8  0x00001C00

/* PPMPC_SF_MASK definitions */
#define PPMPC_SF_2   0x00000100
#define PPMPC_SF_4   0x00000200
#define PPMPC_SF_8   0x00000300
#define PPMPC_SF_16  0x00000000

/* PPMPC_2S_MASK definitions */
#define PPMPC_2S_0     0x00000000
#define PPMPC_2S_CCB   0x00000040
#define PPMPC_2S_CFBD  0x00000080
#define PPMPC_2S_PDC   0x000000C0

/* PPMPC_2D_MASK definitions */
#define PPMPC_2D_1  0x00000000
#define PPMPC_2D_2  0x00000001


struct MADAMDatum
{
	unsigned char PBUSQueue[20];
	unsigned short PLUT[32];
	int RMOD;
	int WMOD;
	unsigned int mregs[2048+64]; 
	unsigned int _madam_FSM;	 
};

static MADAMDatum madam;

unsigned int Get_madam_FSM(){return madam._madam_FSM;};
void Set_madam_FSM(unsigned int val){madam._madam_FSM=val;};

unsigned int _madam_SaveSize()
{
        return sizeof(MADAMDatum);
}
void _madam_Save(void *buff)
{
        memcpy(buff,&madam,sizeof(MADAMDatum));
}
void _madam_Load(void *buff)
{
        memcpy(&madam,buff,sizeof(MADAMDatum));
}

#define mregs madam.mregs
#define PLUT madam.PLUT
#define PBUSQueue madam.PBUSQueue
#define RMOD madam.RMOD
#define WMOD madam.WMOD
#define _madam_FSM madam._madam_FSM
//*******************************************



 unsigned int PXOR1, PXOR2;


#define PDV(x) ((((x)-1)&3)+1)


#define MIN(x,y) (x)+(((signed int)((y)-(x))>>31&((y)-(x))))
#define MAX(x,y) (y)-(((signed int)((y)-(x))>>31&((y)-(x))))

#define TESTCLIP(cx,cy) ( ((int)cx>=0)&&((int)cx<=((CLIPXVAL)<<16))&&((int)cy>=0)&&((int)cy<=((CLIPYVAL)<<16)))

#define FLT(a) ((float)(int)(a)/65536.0)
#define  XY2OFF(a,b,c)   (  (((int)b>>1)*c/*bitmap width*/)   + (((int)(b)&1)<<1) +    (a)    )


#define PBMASK 0x80000000
#define KUP    0x08000000
#define KDN    0x10000000
#define KRI    0x04000000
#define KLE    0x02000000
#define KA     0x01000000
#define KB     0x00800000
#define KC     0x00400000
#define KP     0x00200000
#define KX     0x00100000
#define KRS    0x00080000
#define KLS    0x00040000
#define FIXP16_SHIFT     16
#define FIXP16_MAG       65536
#define FIXP16_DP_MASK   0x0000ffff
#define FIXP16_WP_MASK   0xffff0000
#define FIXP16_ROUND_UP  0x0000ffff //0x8000



// TYPES ///////////////////////////////////////////////////////////////////


// CLASSES /////////////////////////////////////////////////////////////////
unsigned int  mread(unsigned int addr);
void  mwrite(unsigned int addr, unsigned int val);
int TestInitVisual(int packed);
int Init_Line_Map();
void Init_Scale_Map();
void Init_Arbitrary_Map();
int  TexelDraw_Line(unsigned short CURPIX, unsigned short LAMV, int xcur, int ycur, int cnt);
int  TexelDraw_Scale(unsigned short CURPIX, unsigned short LAMV, int xcur, int ycur, int deltax, int deltay);
int  TexelDraw_Arbitrary(unsigned short CURPIX, unsigned short LAMV, int xA, int yA, int xB, int yB, int xC, int yC, int xD, int yD);
void  DrawPackedCel_New();
void  DrawLiteralCel_New();
void  DrawLRCel_New();
void HandleDMA8();
void DMAPBus();


unsigned int MAPPING;

// general 3D vertex class

#define INT1220(a)	 ((signed int)(a)>>20)
#define INT1220up(a) ((signed int)((a)+(1<<19))>>20)

 static struct
 {
//	int mode;
	unsigned int plutaCCBbits;
	unsigned int pixelBitsMask;
	unsigned int pmodeORmask;
	unsigned int pmodeANDmask;
	bool tmask;
	bool Transparent;   /* hoist per-texel del test de transparencia (opera) */
 } pdec;

 unsigned int pbus=0;
 unsigned char *Mem;
 unsigned int retuval;
 unsigned int BITADDR;
 //static unsigned int * BITPTR;
 //static unsigned int * BITEND;
 
 unsigned int CCBFLAGS,/*PLUTDATA*/PIXC,PRE0,PRE1,TARGETPROJ,SRCDATA,debug;
 int SPRWI,SPRHI;
	unsigned int PLUTF,PDATF,NCCBF;
 //int CELCYCLES,__smallcicles;
 bool ADD;
 //static SDL_Event cpuevent;
 int BITCALC;


 
 int    bitcount; // bit counter
 long   compsize; // size of commpressed!!! in bytes!!! actually pixcount*bpp/8!!!
 unsigned int gFINISH;
 unsigned short RRR;
 int USECEL;

 unsigned int	const BPP[8]={1,1,2,4,6,8,16,1};

 unsigned char PSCALAR[8][4][32];

 unsigned short MAPu8b[256+64], MAPc8bAMV[256+64], MAPc16bAMV[8*8*8+64];


 int currentrow;
 unsigned int bpp;
 int pixcount;
 unsigned int type;
 unsigned int offsetl;
 unsigned int offset;
 //static unsigned int begining;
 unsigned int eor;
  int calcx;
  int nrows;

 unsigned int pix;

 unsigned short ttt;

 unsigned int OFF;

 unsigned int pSource;
 
//AString str;

//CelEngine STATBits
#define STATBITS	mregs[0x28]

#define SPRON		0x10
#define SPRPAU		0x20

//CelEngine Registers
#define SPRSTRT		0x100
#define SPRSTOP		0x104
#define SPRCNTU		0x108
#define SPRPAUS		0x10c

#define CCBCTL0		mregs[0x110]
#define REGCTL0		mregs[0x130]
#define REGCTL1		mregs[0x134]
#define REGCTL2		mregs[0x138]
#define REGCTL3		mregs[0x13c]

#define CLIPXVAL	((int)mregs[0x134]&0x3ff)
#define CLIPYVAL	((int)(mregs[0x134]>>16)&0x3ff)

#define PIXSOURCE	(mregs[0x138])
#define FBTARGET	(mregs[0x13c])

#define CURRENTCCB	mregs[0x5a0]
//next ccb == 0 stop the engine
#define NEXTCCB		mregs[0x5a4]
#define PLUTDATA	mregs[0x5a8]
#define PDATA		mregs[0x5ac]
#define ENGAFETCH	mregs[0x5b0]
#define ENGALEN		mregs[0x5b4]
#define	ENGBFETCH	mregs[0x5b8]
#define ENGBLEN		mregs[0x5bc]
#define PAL_EXP		(&mregs[0x5d0])

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//void MapCoord(poly *pol);
//void RenderPoly();
 
HANDLE madamCELHandle;
int _madam_HandleCEL_Internal();
extern int running;


unsigned int  _madam_Peek(unsigned int addr)
{

//	if((addr>=0x400)&&(addr<=0x53f))
//		printf("#Madam Peek [%X]=%X\n",addr,mregs[addr]);


	if((addr>=0x400)&&(addr<=0x53f))
		{
		   //we need to return actual fifo status!!!!
		return _clio_FIFOStruct(addr);
		}


	if(addr==0x28) // STATUS OF CEL
		{
			switch(_madam_FSM)
			{
				case FSM_IDLE:return 0x0;
				case FSM_SUSPENDED:return 0x30;
				case FSM_INPROCESS:return 0x10;
			}
		}
	//if(addr>=0x580 && addr<0x5A0)
	//{
		//sprintf(str,"CLUT - MADAM Read madam[0x%X]\n",addr);
		//CDebug::DPrint(str);
	//}
	return mregs[addr];
}


void  _madam_Poke(unsigned int addr, unsigned int val)
{
 //if(addr>0x2ff && addr<0x400)
 //{
      //  io_interface(EXT_DEBUG_PRINT,(void*)str.print("MADAM Write madam[0x%X] = 0x%8.8X\n",addr,val).CStr());
 //}
 /*
 if(addr==0x13c)
 {
		sprintf(str,"Switch screen SWI 0x%8X addr 0x%8X\n",last_SWI,val);
		CDebug::DPrint(str);
 }*/

	if((addr>=0x400)&&(addr<=0x53f))
	{

		_clio_SetFIFO(addr,val);

		return;

	}
	else
		switch(addr)
	{
	case 0x4:
		val=0x29;
		mregs[addr]=val;
		break;

	case 0x8:
		mregs[addr]=val;
		HandleDMA8();
		break;
	case 0x580:		 
		_vdl_ProcessVDL(val);	 
		return;
	case 0x584:
	case 0x588:
	case 0x58C:
	case 0x590:
	case 0x594:
	case 0x598:
	case 0x59C:
		//_3do_DPrint(str.print("CLUT - MADAM Write madam[0x%X] = 0x%8.8X\n",addr,val));
		mregs[addr]=val;
		return;

	case 0x0:
	//	io_interface(EXT_KPRINT,(void*)val);
		return;
	case SPRSTRT:
 
		if(_madam_FSM==FSM_IDLE)
		{
			_madam_FSM=FSM_INPROCESS;
			_madam_HandleCEL();
#if !defined (THREADING)
			 Set_madam_FSM(FSM_IDLE);
#endif
		}
 
		return;

	case SPRSTOP:
	 
		_madam_FSM=FSM_IDLE;
	 
		NEXTCCB=0;
		return;

	case SPRCNTU:
 
		if(_madam_FSM==FSM_SUSPENDED)
		{
			_madam_FSM=FSM_INPROCESS;
			_madam_HandleCEL();
#if !defined (THREADING)
			 Set_madam_FSM(FSM_IDLE);
#endif
		}
		return;

	case SPRPAUS:
 
		if(_madam_FSM==FSM_INPROCESS)
			_madam_FSM=FSM_SUSPENDED;
	 
		return;

//Matrix engine macros
#define M00  ((float)(signed int)mregs[0x600])
#define M01  ((float)(signed int)mregs[0x604])
#define M02  ((float)(signed int)mregs[0x608])
#define M03  ((float)(signed int)mregs[0x60C])
#define M10  ((float)(signed int)mregs[0x610])
#define M11  ((float)(signed int)mregs[0x614])
#define M12  ((float)(signed int)mregs[0x618])
#define M13  ((float)(signed int)mregs[0x61C])
#define M20  ((float)(signed int)mregs[0x620])
#define M21  ((float)(signed int)mregs[0x624])
#define M22  ((float)(signed int)mregs[0x628])
#define M23  ((float)(signed int)mregs[0x62C])
#define M30  ((float)(signed int)mregs[0x630])
#define M31  ((float)(signed int)mregs[0x634])
#define M32  ((float)(signed int)mregs[0x638])
#define M33  ((float)(signed int)mregs[0x63C])

#define  V0  ((float)(signed int)mregs[0x640])
#define  V1  ((float)(signed int)mregs[0x644])
#define  V2  ((float)(signed int)mregs[0x648])
#define  V3  ((float)(signed int)mregs[0x64C])

#define Rez0 mregs[0x660]
#define Rez1 mregs[0x664]
#define Rez2 mregs[0x668]
#define Rez3 mregs[0x66C]

//#define Nfrac16 ((__int64)mregs[0x680]<<32|(unsigned int)mregs[0x684])
#define Nfrac16 (((__int64)mregs[0x680]<<32)|(unsigned int)mregs[0x684])

// Matix engine


    case 0x7fc:

		mregs[0x7fc]=0; // Ours matrix engine already ready

		static float Rez0T,Rez1T,Rez2T,Rez3T;
	 
	 
               // io_interface(EXT_DEBUG_PRINT,(void*)str.print("MADAM Write madam[0x%X] = 0x%8.8X\n",addr,val).CStr());

		Rez0=Rez0T;
		Rez1=Rez1T;
		Rez2=Rez2T;
		Rez3=Rez3T;

		switch(val) // Cmd
		{
			case 0: //printf("#Matrix = NOP\n");
				return;   // NOP


			case 1: //multiply a 4x4 matrix of 16.16 values by a vector of 16.16 values



#if defined (XNAMATH)
				{


				XMMATRIX m(M00,M10,M20,M30,
						   M01,M11,M21,M31,
						   M02,M12,M22,M32,
						   M03,M13,M23,M33);


				XMVECTOR v = XMVectorSet(V0,V1,V2,V3);
 
				XMVECTOR vecOut = XMVector4Transform(v,m);


 
				Rez0T = (int)((vecOut.x)/65536.0);
				Rez1T = (int)((vecOut.y)/65536.0);
				Rez2T = (int)((vecOut.z)/65536.0);
				Rez3T=  (int)((vecOut.w)/65536.0);
 
				}
			 
#else
				
 
				Rez0T=(int)((M00*V0+M01*V1+M02*V2+M03*V3)/65536.0);
				Rez1T=(int)((M10*V0+M11*V1+M12*V2+M13*V3)/65536.0);
				Rez2T=(int)((M20*V0+M21*V1+M22*V2+M23*V3)/65536.0);
				Rez3T=(int)((M30*V0+M31*V1+M32*V2+M33*V3)/65536.0);
				
			 
				//printf("#Matrix CMD1,R3=0x%8.8X - R3=0x%8.8X\n\n", m._14, M03);

				 
#endif
				return;
			case 2: //multiply a 3x3 matrix of 16.16 values by a vector of 16.16 values
 
#if defined (XNAMATH)
				{

					const XMFLOAT3X3 m(M00,M10,M20,
							           M01,M11,M21,
								       M02,M12,M22);

					XMVECTOR v = XMVectorSet(V0,V1,V2,0); 
					XMMATRIX mOut = XMLoadFloat3x3(&m);
					XMVECTOR vecOut = XMVector3Transform(v,mOut);
 
					Rez0T = (int)((vecOut.x)/65536.0);
					Rez1T = (int)((vecOut.y)/65536.0);
					Rez2T = (int)((vecOut.z)/65536.0);
				 


				}
#else
				Rez0T=(int)((M00*V0+M01*V1+M02*V2)/65536.0);
				Rez1T=(int)((M10*V0+M11*V1+M12*V2)/65536.0);
				Rez2T=(int)((M20*V0+M21*V1+M22*V2)/65536.0);

#endif

				//printf("#Matrix CMD2, R0=0x%8.8X, R1=0x%8.8X, R2=0x%8.8X\n",Rez0,Rez1,Rez2);
				return;

			case 3: // Multiply a 3x3 matrix of 16.16 values by multiple vectors, then multiply x and y by n/z
				{   // Return the result vectors {x*n/z, y*n/z, z}

 
					double M;

					Rez2T=(signed int)((M20*V0+M21*V1+M22*V2)/65536.0); // z
					if(Rez2T!=0)
						M=Nfrac16/(double)Rez2T;          // n/z
					else
						{
							M=Nfrac16;
						//	io_interface(EXT_DEBUG_PRINT,(void*)"!!!Division by zero!!!\n");
						}

					Rez0T=(signed int)((M00*V0+M01*V1+M02*V2)/65536.0);
					Rez1T=(signed int)((M10*V0+M11*V1+M12*V2)/65536.0);


					Rez0T=(double)((Rez0T*M)/65536.0/65536.0); // x * n/z
					Rez1T=(double)((Rez1T*M)/65536.0/65536.0); // y * n/z

				}
				return;
				default:
					//io_interface(EXT_DEBUG_PRINT,(void*)str.print("??? Unknown cmd MADAM[0x7FC]==0x%x\n", val).CStr());
					return;
		}
        break;
	case 0x130:
		mregs[addr]=val;	//modulo variables :)
		RMOD=((val&1)<<7)+((val&12)<<8)+((val&0x70)<<4);
		val>>=8;
		WMOD=((val&1)<<7)+((val&12)<<8)+((val&0x70)<<4);
		break;
	default:
		mregs[addr]=val;
		break;
	}


}
 
unsigned int temp1;
unsigned int Flag;

double HDDX,HDDY,HDX,HDY,VDX,VDY,XPOS,YPOS,HDX_2,HDY_2;

int HDDX1616,HDDY1616,HDX1616,HDY1616,VDX1616,VDY1616,XPOS1616,YPOS1616,HDX1616_2,HDY1616_2;
char	TEXEL_FUN_NUMBER;
int TEXTURE_WI_START,TEXTURE_HI_START,TEXEL_INCX,TEXEL_INCY;
int TEXTURE_WI_LIM, TEXTURE_HI_LIM;


__forceinline void LoadPLUT(unsigned int pnt,int n)
{
	int i;

 	for(i=0;i<n;i++)
	{
		PLUT[i]=_mem_read16((((pnt>>1)+i))<<1);
	}
}

int CCBCOUNTER;

static volatile LONG g_cel_triggers = 0;   /* diag: veces que el ARM dispara el CEL (SPRON) */

int _madam_HandleCEL()
{
	InterlockedIncrement(&g_cel_triggers);
#if defined (THREADING)
	// Tell the thread we need to do some work
	SetEvent(madamCELHandle);
#else
	_madam_HandleCEL_Internal();
#endif

    return 0;
} 

int _madam_HandleCEL_Internal()
{

if(NEXTCCB!=0)CCBCOUNTER=0;
			STATBITS|=SPRON;
		Flag=0;

		while((NEXTCCB!=0)&&(!Flag))
		{
		  
			int OFFSET = 0;
			CCBCOUNTER++;

#if defined (THREADING)
			if (running == 0)
				_endthread();
#endif
			if((NEXTCCB==0)||(Flag))
			{		 
				_madam_FSM=FSM_IDLE;		 
				break;
			}
 
			
			//1st step -- parce CCB and load it into registers
			CURRENTCCB=NEXTCCB&0xfffffc;
			if((CURRENTCCB>>20)>2)
			{			 
				_madam_FSM=FSM_IDLE;			 
				break;
			}
			OFFSET=CURRENTCCB;


			CCBFLAGS=mread(OFFSET);

			OFFSET+=4;


			PXOR1=0xFFffFFff;
			PXOR2=0;

			if(CCBFLAGS&CCB_PXOR)
			{
				PXOR1=0;
				PXOR2=0x1f1f1f1f;
			}
 
			Flag=0;
			PLUTF=PDATF=NCCBF=0;

			NEXTCCB=mread(OFFSET)&(~3);


			if(!(CCBFLAGS&CCB_NPABS))
			{
					NEXTCCB+=OFFSET+4;
					NEXTCCB&=0xffffff;
			}
			if((NEXTCCB==0))
				NCCBF=1;
			if((NEXTCCB>>20)>2)
				NCCBF=1;

			OFFSET+=4;


			PDATA=mread(OFFSET)&(~3);
			//if((PDATA==0))
			//	PDATF=1;
			if(!(CCBFLAGS&CCB_SPABS))
			{
				PDATA+=OFFSET+4;
				PDATA&=0xffffff;
			}
			if((PDATA>>20)>2)
				PDATF=1;
			OFFSET+=4;

			if((CCBFLAGS&CCB_LDPLUT))
			{
				   PLUTDATA=mread(OFFSET)&(~3);
				   //if((PLUTDATA==0))
				   //    PLUTF=1;
				   if(!(CCBFLAGS&CCB_PPABS))
				   {
					   PLUTDATA+=OFFSET+4;
					   PLUTDATA&=0xffffff;
				   }
				   if((PLUTDATA>>20)>2)
					   PLUTF=1;
			}
			OFFSET+=4;


			if(NCCBF)
				CCBFLAGS|=CCB_LAST;


			if(CCBFLAGS&CCB_LAST)
			{
				Flag=1;
				NEXTCCB=0;
			}

			if(CCBFLAGS&CCB_YOXY)
			{
				XPOS1616=mread(OFFSET);
							XPOS=XPOS1616/65536.0;
				//CURRENTCCB+=4;
				YPOS1616=mread(OFFSET+4);
							YPOS=YPOS1616/65536.0;
			 
			}
		 
			OFFSET+=8;

			//if((CCBFLAGS&CCB_SKIP)&& debug)
			//	printf("###Cel skipped!!! PDATF=%d PLUTF=%d NCCBF=%d\n",PDATF,PLUTF,NCCBF);


				//if(CCBFLAGS&CCB_LAST)
				//	NEXTCCB=0;
				if(CCBFLAGS&CCB_LDSIZE)
				{
					HDX1616=((int)mread(OFFSET))>>4;
									HDX=HDX1616/65536.0;
				 
					HDY1616=((int)mread(OFFSET+4))>>4;
									HDY=HDY1616/65536.0;
			 
					VDX1616=mread(OFFSET+8);
									VDX=VDX1616/65536.0;
				 
					VDY1616=mread(OFFSET+12);
									VDY=VDY1616/65536.0;
					OFFSET+=16;
				}
				if(CCBFLAGS&CCB_LDPRS)
				{
					HDDX1616=((int)mread(OFFSET))>>4;
									HDDX=HDDX1616/65536.0;
			 
					HDDY1616=((int)mread(OFFSET+4))>>4;
									HDDY=HDDY1616/65536.0;
					OFFSET+=8;
				}
				if(CCBFLAGS&CCB_LDPPMP)
				{
					PIXC=mread(OFFSET);
					OFFSET+=4;
				}
				if(CCBFLAGS&CCB_CCBPRE)
				{
					PRE0=mread(OFFSET);
					OFFSET+=4;
					if(!(CCBFLAGS&CCB_PACKED))
					{
						PRE1=mread(OFFSET);
						OFFSET+=4;
					}
				}
				else if(!PDATF)
				{
					PRE0=mread(PDATA);
					PDATA+=4;
					if(!(CCBFLAGS&CCB_PACKED))
					{
						PRE1=mread(PDATA);
						PDATA+=4;
					}
				}

				{// PDEC data compute
					//pdec.mode=PRE0&PRE0_BPP_MASK;
					switch(PRE0&PRE0_BPP_MASK)
						{
							case 0:
							case 7:
								continue;
							case 1:
								pdec.plutaCCBbits=(CCBFLAGS&0xf)*4;
								pdec.pixelBitsMask=1; // 1 bit
								break;
							case 2:
								pdec.plutaCCBbits=(CCBFLAGS&0xe)*4;
								pdec.pixelBitsMask=3; // 2 bit
								break;
							default://case 3:
								pdec.plutaCCBbits=(CCBFLAGS&0x8)*4;
								pdec.pixelBitsMask=15; // 4 bit
								break;
					}
					pdec.tmask=!(CCBFLAGS&CCB_BGND);
					pdec.pmodeORmask= ((CCBFLAGS&CCB_POVER_MASK)==PMODE_ONE )? 0x8000:0x0000;
					pdec.pmodeANDmask=((CCBFLAGS&CCB_POVER_MASK)!=PMODE_ZERO)? 0xFFFF:0x7FFF;

				}

				if((CCBFLAGS&CCB_LDPLUT) && !PLUTF) //load PLUT
				{
				   switch(PRE0&PRE0_BPP_MASK)
				   {
				   case 1:
					   LoadPLUT(PLUTDATA,2);
					   break;
				   case 2:
					   LoadPLUT(PLUTDATA,4);
					   break;
				   case 3:
					   LoadPLUT(PLUTDATA,16);
					   break;
				   default:
					   LoadPLUT(PLUTDATA,32);
				   };
				}

			//ok -- CCB decoded -- let's print out our current status
			//step#2 -- getting CEL data
						//*

		
			if(!(CCBFLAGS&CCB_SKIP) && !PDATF)
			{
				if(CCBFLAGS&CCB_PACKED)
				{
					DrawPackedCel_New();
				}
				else
				{

					if((PRE1&PRE1_LRFORM)&&(BPP[PRE0&PRE0_BPP_MASK]==16))
						DrawLRCel_New();
					
					else
						DrawLiteralCel_New();

				}
			
 

			}//if(!(CCBFLAGS& CCB_SKIP))

		}//while 

 
		if((NEXTCCB==0)||(Flag))
		{
	 
			_madam_FSM=FSM_IDLE;
		 
		}

		return 0;
}


#if defined (THREADING)
// Thread Function to handle the heavy lifting of MADAM
// this gives us a significant FPS increase!

/* --- Instrumentacion de rendimiento del CEL (diagnostico ARM vs CEL) --- */
static volatile LONG g_cel_us      = 0;   /* microsegundos acumulados en el blitter CEL */
static volatile LONG g_cel_renders = 0;   /* nº de renders CEL completados              */

/* Lee y resetea atomicamente los acumuladores (lo llama retro_run ~1x/seg). */
extern "C" void _madam_GetCelStats(long *out_us, long *out_renders, long *out_triggers)
{
	*out_us       = InterlockedExchange(&g_cel_us, 0);
	*out_renders  = InterlockedExchange(&g_cel_renders, 0);
	*out_triggers = InterlockedExchange(&g_cel_triggers, 0);
}

unsigned int __stdcall madam_thread_func(LPVOID eventHandle)
{
	HANDLE evHandle = (LPVOID *)eventHandle;

	// CEL engine -> core 1, hardware thread 2 (must NOT be core 2 / HW4-HW5:
	// SDL's audio thread busy-spins on HW5 and would starve us via SMT).
	XSetThreadProcessor( GetCurrentThread(), 2 );

	LARGE_INTEGER _celFreq, _celT0, _celT1;
	QueryPerformanceFrequency(&_celFreq);

	while(1)

	{
		WaitForSingleObject(evHandle,INFINITE);
		QueryPerformanceCounter(&_celT0);
		_madam_HandleCEL_Internal();
		QueryPerformanceCounter(&_celT1);
		InterlockedExchangeAdd(&g_cel_us, (LONG)(((_celT1.QuadPart - _celT0.QuadPart) * 1000000) / _celFreq.QuadPart));
		InterlockedIncrement(&g_cel_renders);
	}

} 

#endif

void HandleDMA8()
{
	if(mregs[0x8]&0x8000)// pbus transfer
	{
		DMAPBus();
		mregs[0x8]&=~0x8000; // dma done

		_clio_GenerateFiq(0,1);
	}
}


void DMAPBus()
{
    unsigned int i=0;

	if((int)mregs[0x574]<0)
		return;

	mregs[0x574]-=4;
	mregs[0x570]+=4;
	mregs[0x578]+=4;

	while((int)mregs[0x574]>0)
	{
			if(i<5) WriteIO(mregs[0x570],((unsigned int*)PBUSQueue)[i]);
			else WriteIO(mregs[0x570],0xffffffff);
			mregs[0x574]-=4;
			mregs[0x570]+=4;
			mregs[0x578]+=4;
			i++;
	}

	mregs[0x574]=0xfffffffc;

	
}

void _madam_KeyPressed(unsigned char* data, unsigned int num)
{
	if(num>16)num=16;
	if(num)memcpy(PBUSQueue,data,num);
	memset(&PBUSQueue[num],-1,20-num);
}


void _madam_Init(unsigned char *memory)
{
	int i,j,n;
	ADD=0;
	debug=0;
	USECEL=1;
	//CELCYCLES=0;
	Mem=memory;

	MAPPING=1;

	//InitializeCriticalSection(&cs);

 
	_madam_FSM=FSM_IDLE;
 

	for(i=0;i<2048;i++)
		mregs[i]=0;

    mregs[004]=0x29;		// DRAM dux init
    mregs[574]=0xfffffffc;

#if 1
	mregs[000]=0x01020000; // for Green matrix engine autodetect
        //mregs[000]=0x02022000; // for Green matrix engine autodetect
#else
	mregs[000]=0x01020001; // for ARM soft emu of matrix engine
#endif



	for(i=0;i<32;i++)
		for(j=0;j<8;j++)
			for(n=0;n<4;n++)
			{
				PSCALAR[j][n][i]=((i*(j+1))>>PDV(n));
			}

	for(i=0;i<256;i++)
	{
		unsigned int	pix1;
		unsigned short pres, resamv;

			pix1=i;
			unsigned short b=((pix1 & 0x3) <<3)+((pix1 & 0x3)<<1)+((pix1 & 0x3)>>1);
			unsigned short g=(((pix1 & 0x1C) >> 2)<<2)+(((pix1 & 0x1C) >> 2)>>1);
			unsigned short r=(((pix1 & 0xE0)>> 5)<<2)+(((pix1 & 0xE0) >> 5)>>1);

			//pix2.r16b.b=(pix1.u8b.b<<3)+(pix1.u8b.b<<1)+(pix1.u8b.b>>1);
			//pix2.r16b.g=(pix1.u8b.g<<2)+(pix1.u8b.g>>1);
			//pix2.r16b.r=(pix1.u8b.r<<2)+(pix1.u8b.r>>1);

			pres=(unsigned short)(b | g << 5 | r << 5);
			pres&=0x7fff; //pmode=0;
			MAPu8b[i]=pres;

			resamv=(unsigned short)((((pix1 & 0xC0) >> 6)<<1)+ ((pix1 & 0x20) >> 5));
			resamv=(resamv<<6)+(resamv<<3)+resamv;
			MAPc8bAMV[i]=resamv;
	}
	for(i=0;i<(8*8*8);i++)
	{
		unsigned int pix1;

			pix1=i<<5;
			MAPc16bAMV[i]=(unsigned short)(( ((pix1 & 0x3800)>> 11) <<6 ) +(((pix1 & 0x700) >> 8) <<3)+((pix1 & 0xE0) >> 5));
	}
		 
#if defined (THREADING)
	/* Create the CEL worker thread ONCE. _madam_Init runs again on every game
	   change / reset (via _3do_Init), and this thread's while(1) cannot be
	   stopped, so re-creating it would leak duplicate CEL workers. It is
	   event-driven and stays idle between games, so it survives reloads. */
	if (!madamCELHandle)
	{
		madamCELHandle = CreateEvent(NULL,0,0,NULL);
		_beginthreadex(NULL,0,&madam_thread_func,(void *)madamCELHandle,0,NULL);
	}
#endif

}

extern void _3do_InternalFrame(int cicles);

#if 0
void exteraclocker()
{
        if((CELCYCLES-__smallcicles)>>7)
        {
                __smallcicles=CELCYCLES;
                //_3do_InternalFrame(64);
        }
}
#endif

unsigned int  mread(unsigned int addr)
{
	unsigned int val;
#ifdef SAFEMEMACCESS
//	addr&=0x3FFFFF;
#endif

	val=_mem_read32(addr);
	//CELCYCLES+=1;
        //exteraclocker();
	return val;
}

void  mwrite(unsigned int addr, unsigned int val)
{
#ifdef SAFEMEMACCESS
	addr&=0x3FFFFF;
#endif
	_mem_write32(addr,val);
	//CELCYCLES+=2;
        //exteraclocker();

}

/* Per-pixel CEL VRAM write. INLINED + acceso directo a Mem: pasar por
   _mem_write16 (en arm.cpp) era una llamada CROSS-TU que NO inlina con LTCG
   apagado -> ~2 llamadas/pixel solo para escribir. Mem == base de pRam
   (fijada en _madam_Init), asi que Mem[addr] == pRam[addr]. */
static __forceinline void mwriteh(unsigned int addr, unsigned short val)
{
#ifdef SAFEMEMACCESS
	addr&=0x3fffff;
#endif
#if defined (BIG_ENDIAN)
	*((unsigned short*)&Mem[addr])   = val;
#else
	*((unsigned short*)&Mem[addr^2]) = val;
#endif

        //exteraclocker();
}

/* Per-pixel CEL VRAM read. INLINED + acceso directo a Mem (ver mwriteh). */
static __forceinline unsigned short mreadh(unsigned int addr)
{
#if defined (BIG_ENDIAN)
	return *((unsigned short*)&Mem[addr]);
#else
	return *((unsigned short*)&Mem[addr^2]);
#endif
}

 
__inline unsigned int  PDEC(unsigned int pixel, unsigned short * amv)
{

	unsigned int	pix1;
	unsigned short resamv,pres;	

	//pixraw = pixel;
	pix1= pixel;
 
	switch(PRE0&PRE0_BPP_MASK/*pdec.mode*/)
	{
		default:
		//case 1: // 1 bit
		//case 2: // 2 bits
		//case 3: // 4 bits


			pres=PLUT[(pdec.plutaCCBbits+((pix1&pdec.pixelBitsMask)*2))>>1];
			//felix disable

			resamv=0x49;
			break;

		case 4:   // 6 bits
			
			pres=PLUT[pix1 & 0x1F];
			pres=(pres&0x7FFF)+(((pix1 & 0x20) >> 5) << 15); //pmode=pix1.c6b.pw; ???

			resamv=0x49;
			break;

		case 5:   // 8 bits

			if(PRE0&PRE0_LINEAR)
			{
				pres=MAPu8b[pix1&0xFF];

				resamv=0x49;
			}
			else
			{
				pres=PLUT[pix1 & 0x1F];

				resamv=MAPc8bAMV[pix1&0xFF];
			}
			break;

		case 6:  // 16 bits
		case 7:
			//*amv=0;
			//pres=0;
		    //Transparent=0;
			if((PRE0&PRE0_LINEAR))
			{

				pres=pix1;
				//pres&=0x7ffe;

				//pres=0x11;
				// pres=(pres&0x7fff)+(pix1.u16b.p<<15);//pmode=pix1.u16b.p; ???
				resamv=0x49;

			}
			else
			{
				pres=PLUT[pix1 & 0x1F];
				pres=(pres&0x7fff)|(pix1&0x8000);
				resamv=MAPc16bAMV[(pix1>>5)&0x1FF];
				//nop: pres=(pres&0x7fff)+(pix1.c16b.pw<<15);//pmode=pix1.c16b.pw; ???
			}

			break;
	}

	*amv=resamv;

	//if(!(PRE1&PRE1_NOSWAP) && (CCBCTL0&(1<<27)))
	//			pres=(pres&0x7ffe)|((pres&0x8000)>>15)|((pres&1)<<15);

	//if(!(CCBCTL0&0x80000000))pres=(pres&0x7fff)|((CCBCTL0>>15)&0x8000);

	pres=(pres|pdec.pmodeORmask)&pdec.pmodeANDmask;


	/* hoist (opera): calcula la transparencia una vez por texel aqui; los ~12
	   sitios de dibujo solo consultan pdec.Transparent en vez de recomputar
	   (CURPIX&0x7fff)==0 & tmask por pixel. Mismo valor -> salida identica. */
	pdec.Transparent = ( ((pres&0x7fff)==0x0) & pdec.tmask );

	return pres;

}

/* --- Hoist de PPROC (opcion 3dox_cel_hoist) ------------------------------------
   Dentro de UNA llamada a TexelDraw_*, pixel(=CURPIX), amv, PIXC y CCBFLAGS son
   CONSTANTES; solo fpix (el pixel leido) varia por pixel de destino. Estos helpers
   factorizan EXACTAMENTE las mismas formulas que el camino de referencia de PPROC
   (abajo), para poder precalcular una sola vez por llamada el control de PIXC y
   color1 (los 3 lookups de PSCALAR) cuando son invariantes -> se ahorran por pixel.
   Salida bit-identica al camino de referencia. */
static __forceinline unsigned int pp_color1(unsigned int input1, unsigned int pixel, unsigned int amv,
                                             unsigned int MSmode, unsigned char mxf, unsigned char dv1)
{
	unsigned int color1 = 0, pix1;
	unsigned short r = (unsigned short)((input1 & 0x7C00) >> 10);
	unsigned short g = (unsigned short)((input1 & 0x03E0) >> 5);
	unsigned short b = (unsigned short)input1 & 0x1F;
	switch(MSmode)
	{
	case 0:
		color1 = PSCALAR[mxf][dv1][b] << 16 | PSCALAR[mxf][dv1][g] << 8| PSCALAR[mxf][dv1][r];
		break;
	case 1:
		color1 = PSCALAR[amv&7][dv1][b] << 16 | PSCALAR[(amv>>3)&7][dv1][g] << 8| PSCALAR[(amv>>6)&7][dv1][r];
		break;
	case 2:
		{
			pix1=pixel;
			unsigned short pix1r = (unsigned short)((pix1 & 0x7C00) >> 10);
			unsigned short pix1g = (unsigned short)((pix1 & 0x03E0) >> 5);
			unsigned short pix1b = (unsigned short)pix1 & 0x1F;
			color1 = PSCALAR[pix1b>>2][pix1b&3][b] << 16 | PSCALAR[pix1g>>2][pix1g&3][g] << 8 | PSCALAR[pix1r>>2][pix1r&3][r];
		}
		break;
	case 3:
		color1 = PSCALAR[4][dv1][b] << 16 | PSCALAR[4][dv1][g] << 8 | PSCALAR[4][dv1][r];
		break;
	}
	return color1;
}
static __forceinline unsigned int pp_color2(unsigned int S2mode, unsigned int pixel, unsigned int fpix,
                                             unsigned char pcAV, unsigned char dv3)
{
	unsigned int color2 = 0, pix1;
	switch(S2mode)
	{
	case 0:
		color2 = 0;
		break;
	case 1:
		{
			unsigned char av = pcAV;
			color2 = (av>>dv3) << 16 | (av>>dv3) << 8 | (av>>dv3);
		}
		break;
	case 2:
		{
			pix1=fpix;
			unsigned short r = (unsigned short)((pix1 & 0x7C00) >> 10);
			unsigned short g = (unsigned short)((pix1 & 0x03E0) >> 5);
			unsigned short b = (unsigned short)pix1 & 0x1F;
			color2 =  ((b)>>(dv3) << 16) | ((g)>>(dv3) << 8)  | ((r)>>(dv3)) ;
		}
		break;
	case 3:
		{
			pix1=pixel;
			unsigned short r = (unsigned short)((pix1 & 0x7C00) >> 10);
			unsigned short g = (unsigned short)((pix1 & 0x03E0) >> 5);
			unsigned short b = (unsigned short)pix1 & 0x1F;
			color2 =  ((b)>>(dv3) << 16) | ((g)>>(dv3) << 8)  | ((r)>>(dv3)) ;
		}
		break;
	}
	return color2;
}
static __forceinline unsigned int pp_combine(unsigned int color1, unsigned int color2, unsigned int AV, unsigned char dv2)
{
	unsigned int AOP, BOP;
	AOP = color1 & PXOR1;
	color1 &= PXOR2;
	unsigned char BOPr = 0, BOPg = 0, BOPb = 0;
	unsigned char AOPr = (unsigned char)(AOP & 0xFF);
	unsigned char AOPg = (unsigned char)((AOP >> 8) & 0xFF);
	unsigned char AOPb = (unsigned char)((AOP >> 16) & 0xFF);
	if(AV & 0x1)
	{
		BOP=color2^0x00ffffff;
		BOPr = (unsigned char)(BOP & 0xFF);
		BOPg = (unsigned char)((BOP >> 8) & 0xFF);
		BOPb = (unsigned char)((BOP >> 16) & 0xFF);
	}
	else
	{
		BOP=color2^color1;
		BOPr = (unsigned char)(BOP & 0xFF);
		BOPg = (unsigned char)((BOP >> 8) & 0xFF);
		BOPb = (unsigned char)((BOP >> 16) & 0xFF);
	}
	if((AV & 3) >> 1)
	{
		BOPr=(BOPr<<3)>>3;
		BOPb=(BOPb<<3)>>3;
		BOPg=(BOPg<<3)>>3;
	}
	unsigned char COL2r = 0, COL2g = 0, COL2b = 0;
	COL2r = (((AOPr) + BOPr +  (AV & 0x01)) >> dv2);
	COL2g = (((AOPg) + BOPg +  (AV & 0x01)) >> dv2);
	COL2b = (((AOPb) + BOPb +  (AV & 0x01)) >> dv2);
	if(!((AV & 0x04) >> 2))
	{
		if(COL2r>31) COL2r=31;
		if(COL2g>31) COL2g=31;
		if(COL2b>31) COL2b=31;
	}
	return COL2r << 10 | COL2g << 5 | COL2b;
}

/* Toggle del hoist + cache por-llamada (monohilo). Clave = (pixel,amv,PIXC,CCBFLAGS);
   dentro de una llamada a TexelDraw es constante -> se recalcula solo en el 1er pixel. */
int g_cel_hoist = 0;
static unsigned int g_pp_pixel=0xFFFFFFFFu, g_pp_amv=0xFFFFFFFFu, g_pp_PIXC=0xFFFFFFFFu, g_pp_flags=0xFFFFFFFFu;
static unsigned int g_pp_AV, g_pp_dv2, g_pp_dv3, g_pp_S1sel, g_pp_S2mode, g_pp_MSmode, g_pp_mxf, g_pp_dv1, g_pp_pcAV;
static int g_pp_c1const, g_pp_c2const;
static unsigned int g_pp_color1, g_pp_color2;

void _madam_SetCelHoist(int on){ g_cel_hoist = (on != 0); }

__inline unsigned int PPROC(unsigned int pixel, unsigned int fpix, unsigned int amv)
{

	// optimized for PPC
	// bitfields are horrible, just say no

	unsigned int color1 = 0;
	unsigned int color2 = 0;
	unsigned int pixc = 0;
	unsigned int pix1 = 0;
	unsigned int input1 = 0;
	unsigned int out = 0;
	unsigned int AV = 0;
	 
	unsigned int AOP = 0;
	unsigned int BOP = 0;
	
	//return pixel;

	if (g_cel_hoist)
	{
		if (pixel != g_pp_pixel || amv != g_pp_amv || PIXC != g_pp_PIXC || CCBFLAGS != g_pp_flags)
		{
			unsigned int  pc   = (pixel & 0x8000) ? (PIXC >> 16) : (PIXC & 0xffff);
			unsigned char pcAV = (unsigned char)((pc & 0x3E) >> 1);
			unsigned int  av   = (CCBFLAGS & CCB_USEAV) ? pcAV : 0;
			g_pp_pixel = pixel; g_pp_amv = amv; g_pp_PIXC = PIXC; g_pp_flags = CCBFLAGS;
			g_pp_AV     = av;
			g_pp_dv2    = pc & 1;
			g_pp_dv3    = (av & 0x18) >> 3;
			g_pp_S1sel  = (pc & 0x8000) >> 15;
			g_pp_S2mode = (pc & 0xC0) >> 6;
			g_pp_MSmode = (pc & 0x2000) >> 13;
			g_pp_mxf    = (pc & 0x1C00) >> 10;
			g_pp_dv1    = (pc & 0x300) >> 8;
			g_pp_pcAV   = pcAV;
			g_pp_c1const = !g_pp_S1sel;   /* input1 = pixel (constante) -> color1 constante */
			if (g_pp_c1const)
				g_pp_color1 = pp_color1(pixel, pixel, amv, g_pp_MSmode, (unsigned char)g_pp_mxf, (unsigned char)g_pp_dv1);
			g_pp_c2const = (g_pp_S2mode != 2);   /* solo CFBD (caso 2) usa fpix */
			if (g_pp_c2const)
				g_pp_color2 = pp_color2(g_pp_S2mode, pixel, 0, (unsigned char)g_pp_pcAV, (unsigned char)g_pp_dv3);
		}

		input1 = g_pp_S1sel ? fpix : pixel;
		color1 = g_pp_c1const ? g_pp_color1 : pp_color1(input1, pixel, amv, g_pp_MSmode, (unsigned char)g_pp_mxf, (unsigned char)g_pp_dv1);
		color2 = g_pp_c2const ? g_pp_color2 : pp_color2(2, pixel, fpix, (unsigned char)g_pp_pcAV, (unsigned char)g_pp_dv3);
		return pp_combine(color1, color2, g_pp_AV, (unsigned char)g_pp_dv2);
	}

	pixc=PIXC&0xffff; 
	input1=fpix;

	if((pixel&0x8000))
	{
		pixc=PIXC>>16;
	}

	unsigned char pixcAV = (pixc & 0x3E) >> 1;	

	if(CCBFLAGS&CCB_USEAV)
	{
		AV=pixcAV;
	}
	
	if(!((pixc & 0x8000) >> 15))
	{
		input1=pixel;
	}
	
	const unsigned char dv2 = (pixc & 0x1);

	switch((pixc & 0xC0) >> 6)
	{
	case 0:
		{
			color2=0;
			break;
		}
	case 1:	
		{
			
			unsigned char av = pixcAV;
			unsigned char dv3 = (AV & 0x18) >> 3;

			color2 = (av>>dv3) << 16 | (av>>dv3) << 8 | (av>>dv3);

			break;
		}
	case 2:
		{
			pix1=fpix;
			unsigned short r = (unsigned short)((pix1 & 0x7C00) >> 10);
			unsigned short g = (unsigned short)((pix1 & 0x03E0) >> 5);
			unsigned short b = (unsigned short)pix1 & 0x1F;
			
			unsigned char dv3 = (AV & 0x18) >> 3;

			color2 =  ((b)>>(dv3) << 16) | ((g)>>(dv3) << 8)  | ((r)>>(dv3)) ;	 
			break;
		}
	case 3:
		{
			pix1=pixel;
			unsigned short r = (unsigned short)((pix1 & 0x7C00) >> 10);
			unsigned short g = (unsigned short)((pix1 & 0x03E0) >> 5);
			unsigned short b = (unsigned short)pix1 & 0x1F;

			unsigned char dv3 = (AV & 0x18) >> 3;

			color2 =  ((b)>>(dv3) << 16) | ((g)>>(dv3) << 8)  | ((r)>>(dv3)) ;	 
			break;
			 
		}
	}

	switch((pixc & 0x2000) >> 13)
	{
	case 0:
		{
			unsigned char mxf = (pixc & 0x1C00) >> 10;
			unsigned char dv1 = (pixc & 0x300) >> 8;
			unsigned short r = (unsigned short)((input1 & 0x7C00) >> 10);
			unsigned short g = (unsigned short)((input1 & 0x03E0) >> 5);
			unsigned short b = (unsigned short)input1 & 0x1F;

			color1 = PSCALAR[mxf][dv1][b] << 16 | PSCALAR[mxf][dv1][g] << 8| PSCALAR[mxf][dv1][r];
			 
			break;
		}
	case 1:
		{
			unsigned char dv1 = (pixc & 0x300) >> 8;
			unsigned short r = (unsigned short)((input1 & 0x7C00) >> 10);
			unsigned short g = (unsigned short)((input1 & 0x03E0) >> 5);
			unsigned short b = (unsigned short)input1 & 0x1F;
 
			/* FIX (opera opera_madam.c:1777): R final <- nibble AMV (amv>>6)&7, B final <- amv&7
			   (estaban intercambiados). Slot <<0 = R final, slot <<16 = B final. */
			color1 = PSCALAR[amv&7][dv1][b] << 16 | PSCALAR[(amv>>3)&7][dv1][g] << 8| PSCALAR[(amv>>6)&7][dv1][r];

		break;
		}
	case 2:
		{
			pix1=pixel;

			unsigned short r = (unsigned short)((input1 & 0x7C00) >> 10);
			unsigned short g = (unsigned short)((input1 & 0x03E0) >> 5);
			unsigned short b = (unsigned short)input1 & 0x1F;

			unsigned short pix1r = (unsigned short)((pix1 & 0x7C00) >> 10);
			unsigned short pix1g = (unsigned short)((pix1 & 0x03E0) >> 5);
			unsigned short pix1b = (unsigned short)pix1 & 0x1F;

			color1 = PSCALAR[pix1b>>2][pix1b&3][b] << 16 | PSCALAR[pix1g>>2][pix1g&3][g] << 8 | PSCALAR[pix1r>>2][pix1r&3][r];

			break;
		}
	case 3:
		{
			unsigned short r = (unsigned short)((input1 & 0x7C00) >> 10);
			unsigned short g = (unsigned short)((input1 & 0x03E0) >> 5);
			unsigned short b = (unsigned short)input1 & 0x1F;

			unsigned char dv1 = (pixc & 0x300) >> 8;
 
			color1 = PSCALAR[4][dv1][b] << 16 | PSCALAR[4][dv1][g] << 8 | PSCALAR[4][dv1][r];

			break;
		}
	}
	 
	AOP=color1&PXOR1;	
	color1&=PXOR2;

	unsigned char BOPr = 0;
	unsigned char BOPg = 0;
	unsigned char BOPb = 0;

	unsigned char AOPr = (unsigned char)(AOP & 0xFF);   /* FIX: rojo 8-bit como G/B (era 0x1F) */
	unsigned char AOPg = (unsigned char)((AOP >> 8) & 0xFF);
	unsigned char AOPb = (unsigned char)((AOP >> 16) & 0xFF);

	if(AV & 0x1)
	{
		BOP=color2^0x00ffffff; 
 
		BOPr = (unsigned char)(BOP & 0xFF);   /* FIX: rojo 8-bit como G/B (era 0x1F) */
		BOPg = (unsigned char)((BOP >> 8) & 0xFF);
		BOPb = (unsigned char)((BOP >> 16) & 0xFF);

	}
	else
	{
		BOP=color2^color1;

		BOPr = (unsigned char)(BOP & 0xFF);   /* FIX: rojo 8-bit como G/B (era 0x1F) */
		BOPg = (unsigned char)((BOP >> 8) & 0xFF);
		BOPb = (unsigned char)((BOP >> 16) & 0xFF);

	}


	if((AV & 3) >> 1)
	{ 		
		BOPr=(BOPr<<3)>>3;
		BOPb=(BOPb<<3)>>3;
		BOPg=(BOPg<<3)>>3;
 	 
	}
 
	unsigned char COL2r = 0;
	unsigned char COL2g = 0;
	unsigned char COL2b = 0;


	/* FIX (opera opera_madam.c:1820): aplicar el divisor /DV (PPMPC bit0, dv2).
	   Sin el >>dv2 la translucidez 2D=/2 no se dividia -> HUD/gauges opacos.
	   Se hace sobre la suma int (antes de truncar al unsigned char). */
	COL2r = (((AOPr) + BOPr +  (AV & 0x01)) >> dv2);
	COL2g = (((AOPg) + BOPg +  (AV & 0x01)) >> dv2);
	COL2b = (((AOPb) + BOPb +  (AV & 0x01)) >> dv2);

	if(!((AV & 0x04) >> 2))
	{
			if(COL2r<0)  COL2r=0;
			else if(COL2r>31) COL2r=31;

			if(COL2g<0)  COL2g=0;
			else if(COL2g>31) COL2g=31;

			if(COL2b<0)  COL2b=0;
			else if(COL2b>31) COL2b=31;

	}

	color2 =  COL2r << 10 | COL2g << 5 | COL2b;

	//if(!(CCBFLAGS&CCB_NOBLK) && color2==0) color2=1<<10;

	return color2;
}



unsigned int * _madam_GetRegs()
{
	return mregs;
}



/* Consolidacion (opera process_pixel): read-modify-write del texel en UN sitio.
   Calcula el offset X (a) una sola vez -> antes XY2OFF((xcur>>16)<<2,...) se
   evaluaba 2 veces por pixel (lectura + escritura). __forceinline => mismo
   codegen que el inline previo, salida identica. */
static __forceinline void PlotTexel(int xoff, int yv, unsigned short CURPIX, unsigned short LAMV)
{
	mwriteh((FBTARGET  + XY2OFF(xoff, yv, WMOD)),
	        PPROC(CURPIX, mreadh((PIXSOURCE + XY2OFF(xoff, yv, RMOD))), LAMV));
}

void  DrawPackedCel_New()
{
	sf=100000;
	unsigned int start;
	unsigned short CURPIX,LAMV;
	// RMOD=RMODULO[REGCTL0];
	// WMOD=WMODULO[REGCTL0];
	int lastaddr;
	int xcur,ycur,xvert,yvert,xdown,ydown,hdx,hdy, scipw, wcnt;
	BitReaderBig bitoper;
        //double dxcur,dycur,dxvert,dyvert,dxdown,dydown,dhdx,dhdy;

	start = PDATA;

	nrows=(PRE0&PRE0_VCNT_MASK)>>PRE0_VCNT_SHIFT;

	bpp=BPP[PRE0&PRE0_BPP_MASK];
	offsetl=2;	if(bpp < 8) offsetl=1;

	pixcount=0;


	compsize=30;

	SPRHI=nrows+1;
	calcx=0;

	//SPRWI=0;
	if(TestInitVisual(1))return;
	xvert=XPOS1616;
	yvert=YPOS1616;
	//return;

if(TEXEL_FUN_NUMBER==0)
{
	//return;
	for(currentrow=0;currentrow<(TEXTURE_HI_LIM);currentrow++)
	{

		//Initbitoper.Read(start,300);
		bitoper.AttachBuffer(start);
		offset=bitoper.Read(offsetl<<3);
		//bitoper.Read(offsetl<<3);

		//BITCALC=((offset+2)<<2)<<5;
		lastaddr=start+((offset+2)<<2);
	//	calcx=0;
		eor=0;


		xcur=xvert;
		ycur=yvert;
		xvert+=VDX1616;
		yvert+=VDY1616;
		//if((ycur>>16)>CLIPXVAL)ycur=CLIPXVAL<<16;
		//if(ycur<0)ycur=0;

		if(TEXTURE_HI_START){TEXTURE_HI_START--;start=lastaddr;continue;}
		scipw=TEXTURE_WI_START;
		wcnt=scipw;
		while(!eor)//while not end of row
		{

			type=bitoper.Read(2);//bitoper.Read(2);
                        if( (int)(bitoper.GetBytePose()+start) >= (lastaddr))type=0;

			//pixcount=bitoper.Read(6)+1;
			pixcount=bitoper.Read(6)+1;

			if(scipw)
			{
				if(type==0) break;
				if(scipw>=(int)(pixcount))
				{
					scipw-=(pixcount);
					if(HDX1616)xcur+=HDX1616*(pixcount);
					if(HDY1616)ycur+=HDY1616*(pixcount);
					if(type==1)bitoper.Skip(bpp*pixcount);//bitoper.Skip(bpp*(pixcount));
					else if(type==3)bitoper.Skip(bpp);//bitoper.Skip(bpp);
					continue;
				}
				else
				{
					if(HDX1616)xcur+=HDX1616*(scipw);
					if(HDY1616)ycur+=HDY1616*(scipw);
					pixcount-=scipw;
					if(type==1)bitoper.Skip(bpp*scipw);//bitoper.Skip(bpp*scipw);
					scipw=0;
				}
			}
			//if(wcnt>=TEXTURE_WI_LIM)break;
			wcnt+=(pixcount);
			if(wcnt>TEXTURE_WI_LIM)
			{
				pixcount-=(wcnt-TEXTURE_WI_LIM);
				//if(pixcount>>31)break;
			}
			switch(type)
			{
				case 0: //end of row
					eor=1;
					break;
				case 1: //PACK_LITERAL
					for(pix=0;pix<pixcount;pix++)
					{
						//CURPIX=PDEC(bitoper.Read(bpp),&LAMV);
						CURPIX=PDEC(bitoper.Read(bpp),&LAMV);

						if(!pdec.Transparent)
						{

								//TexelDraw_Line(CURPIX, LAMV, xcur, ycur, 1);
								PlotTexel((xcur>>16)<<2, ycur>>16, CURPIX, LAMV);

						}
						xcur+=HDX1616;
						ycur+=HDY1616;

					}

					break;
				case 2: //PACK_TRANSPARENT

					//	calcx+=(pixcount+1);
					
					if(HDX1616)xcur+=HDX1616*(pixcount);
					if(HDY1616)ycur+=HDY1616*(pixcount);

					break;
				case 3: //PACK_REPEAT

					//CURPIX=PDEC(bitoper.Read(bpp),&LAMV);
					CURPIX=PDEC(bitoper.Read(bpp),&LAMV);
					if(CURPIX>32300&&CURPIX<33500&&(CURPIX>32760||CURPIX<32750)){
					if(speedfixes>=0&&sdf==0&&speedfixes<=200001&&unknownflag11==0)speedfixes=200000;}
					if(unknownflag11>0&&sdf==0&&CURPIX<30000&&CURPIX>29000) speedfixes=-200000;

					if(!pdec.Transparent)
					{

							TexelDraw_Line(CURPIX, LAMV, xcur, ycur, (pixcount));

					}
					if(HDX1616)xcur+=HDX1616*(pixcount);
					if(HDY1616)ycur+=HDY1616*(pixcount);

				break;
			}//type
			if(wcnt>=TEXTURE_WI_LIM)break;
		}//eor

		start=lastaddr;

	}
}
else if(TEXEL_FUN_NUMBER==1)
{ 
	for(currentrow=0;currentrow<SPRHI;currentrow++)
	{

		bitoper.AttachBuffer(start);
		offset=bitoper.Read(offsetl<<3);

		BITCALC=((offset+2)<<2)<<5;
		lastaddr=start+((offset+2)<<2);

		eor=0;

		xcur=xvert;
		ycur=yvert;
		xvert+=VDX1616;
		yvert+=VDY1616;

		while(!eor)//while not end of row
		{

			type=bitoper.Read(2);
                        if( (bitoper.GetBytePose()+start) >= (lastaddr))type=0;

			int __pix=bitoper.Read(6)+1;
			switch(type)
			{
				case 0: //end of row
					eor=1;
					break;
				case 1: //PACK_LITERAL

					while(__pix)
					{
                                                __pix--;
						CURPIX=PDEC(bitoper.Read(bpp),&LAMV);

						if(!pdec.Transparent)
						{

								if(TexelDraw_Scale(CURPIX, LAMV, xcur>>16, ycur>>16, (xcur+(HDX1616+VDX1616))>>16, (ycur+(HDY1616+VDY1616))>>16))break;

						}
						xcur+=HDX1616;
						ycur+=HDY1616;

					}

					break;
				case 2: //PACK_TRANSPARENT

					//	calcx+=(pixcount+1);
					xcur+=HDX1616*(__pix);
					ycur+=HDY1616*(__pix);
                                        __pix=0;

					break;
				case 3: //PACK_REPEAT
					CURPIX=PDEC(bitoper.Read(bpp),&LAMV);
					if(!pdec.Transparent)
					{

							if(TexelDraw_Scale(CURPIX, LAMV, xcur>>16, ycur>>16, (xcur+(HDX1616*(__pix))+VDX1616)>>16, (ycur+(HDY1616*(__pix))+VDY1616)>>16))break;

					}
					xcur+=HDX1616*(__pix);
					ycur+=HDY1616*(__pix);
                                        __pix=0;
				break;
			}//type
			if(__pix)break;
		}//eor


		start=lastaddr;


	}

}
else
{
if(speedfixes>=0&&speedfixes<=100001) speedfixes=100000;
        for(currentrow=0;currentrow<SPRHI;currentrow++)
	{

		bitoper.AttachBuffer(start);
		offset=bitoper.Read(offsetl<<3);

		BITCALC=((offset+2)<<2)<<5;
		lastaddr=start+((offset+2)<<2);

		eor=0;

		xcur=xvert;
		ycur=yvert;
		hdx=HDX1616;
		hdy=HDY1616;

		xvert+=VDX1616;
		yvert+=VDY1616;
		HDX1616+=HDDX1616;
		HDY1616+=HDDY1616;


                xdown=xvert;
		ydown=yvert;

		while(!eor)//while not end of row
		{

			type=bitoper.Read(2);
                        if( (bitoper.GetBytePose()+start) >= (lastaddr))type=0;

			int __pix=bitoper.Read(6)+1;

			switch(type)
			{
				case 0: //end of row
					eor=1;
					break;
				case 1: //PACK_LITERAL

					while(__pix)
					{
						CURPIX=PDEC(bitoper.Read(bpp),&LAMV);
                                                __pix--;
						if(!pdec.Transparent)
						{

								if(TexelDraw_Arbitrary(CURPIX, LAMV, xcur, ycur, xcur+hdx, ycur+hdy, xdown+HDX1616, ydown+HDY1616, xdown, ydown))break;

						}
						xcur+=hdx;
						ycur+=hdy;
						xdown+=HDX1616;
						ydown+=HDY1616;
					}
                                        //pixcount=0;
					break;
				case 2: //PACK_TRANSPARENT

					//	calcx+=(pixcount+1);
					xcur+=hdx*(__pix);
					ycur+=hdy*(__pix);
					xdown+=HDX1616*(__pix);
					ydown+=HDY1616*(__pix);
                                        __pix=0;

					break;
				case 3: //PACK_REPEAT
					CURPIX=PDEC(bitoper.Read(bpp),&LAMV);
					if(speedfixes>=0&&speedfixes<200001&&((CURPIX>10000&&CURPIX<11000)&&sdf==0/*||(CURPIX>10500&&CURPIX<10650)*/))speedfixes=200000;//(CURPIX>10450&&CURPIX<10470)
					if(!pdec.Transparent)
					{
							while(__pix)
                                                        {
                                                                __pix--;
                                                                if(TexelDraw_Arbitrary(CURPIX, LAMV, xcur, ycur, xcur+hdx, ycur+hdy, xdown+HDX1616, ydown+HDY1616, xdown, ydown))break;
                                                                xcur+=hdx;
					                        ycur+=hdy;
					                        xdown+=HDX1616;
					                        ydown+=HDY1616;
                                                        }
					}
                                        else
                                        {
                                                xcur+=hdx*__pix;
                                                ycur+=hdy*__pix;
                                                xdown+=HDX1616*__pix;
                                                ydown+=HDY1616*__pix;
                                                __pix=0;
                                        }
                                        //pixcount=0;

				break;
			};//type
			if(__pix) break;
		}//eor

		start=lastaddr;

	}
}
	SPRWI++;
        XPOS1616=xcur;
        XPOS=XPOS1616/65536.0;
        //YPOS1616=ycur;
}

void  DrawLiteralCel_New()
{
	sf=100000;
	int i,j,xcur,ycur,xvert,yvert,xdown,ydown,hdx,hdy;
 	unsigned short CURPIX,LAMV;

	// RMOD=RMODULO[REGCTL0];
	// WMOD=WMODULO[REGCTL0];

	BitReaderBig bitoper;

	bpp=BPP[PRE0&PRE0_BPP_MASK];
	offsetl=2;	if(bpp < 8)	offsetl=1;
	pixcount=0;
	offset=(offsetl==1)?((PRE1&PRE1_WOFFSET8_MASK)>>PRE1_WOFFSET8_SHIFT):((PRE1&PRE1_WOFFSET10_MASK)>>PRE1_WOFFSET10_SHIFT);


	SPRWI=1+(PRE1&PRE1_TLHPCNT_MASK);
	SPRHI=((PRE0&PRE0_VCNT_MASK)>>PRE0_VCNT_SHIFT)+1;

	if(TestInitVisual(0))return;
	xvert=XPOS1616;
	yvert=YPOS1616;



if(TEXEL_FUN_NUMBER==0)
{
	sdf=100000;
        //������ NFS
        SPRWI-=((PRE0>>24)&0xf);
	xvert+=TEXTURE_HI_START*VDX1616;
	yvert+=TEXTURE_HI_START*VDY1616;
	PDATA+=((offset+2)<<2)*TEXTURE_HI_START;
	if(SPRWI>TEXTURE_WI_LIM)SPRWI=TEXTURE_WI_LIM;
	for(i=TEXTURE_HI_START;i<TEXTURE_HI_LIM;i++)
	{

		bitoper.AttachBuffer(PDATA);
		BITCALC=((offset+2)<<2)<<5;
		xcur=xvert+TEXTURE_WI_START*HDX1616;
		ycur=yvert+TEXTURE_WI_START*HDY1616;
                bitoper.Skip(bpp*(((PRE0>>24)&0xf)));
		if(TEXTURE_WI_START)bitoper.Skip(bpp*(TEXTURE_WI_START));

		xvert+=VDX1616;
		yvert+=VDY1616;


		for(j=TEXTURE_WI_START;j<SPRWI;j++)
		{
			CURPIX=PDEC(bitoper.Read(bpp),&LAMV);

			if(!pdec.Transparent)
			{

					//TexelDraw_Line(CURPIX, LAMV, xcur, ycur, 1);
				PlotTexel((xcur>>16)<<2, ycur>>16, CURPIX, LAMV);



			}
			xcur+=HDX1616;
			ycur+=HDY1616;

		}
		PDATA+=(offset+2)<<2;

	}
}
else if(TEXEL_FUN_NUMBER==1)
{


    SPRWI-=((PRE0>>24)&0xf);
	for(i=0;i<SPRHI;i++)
	{

		bitoper.AttachBuffer(PDATA);
		BITCALC=((offset+2)<<2)<<5;
		xcur=xvert;
		ycur=yvert;
		xvert+=VDX1616;
		yvert+=VDY1616;
                bitoper.Skip(bpp*(((PRE0>>24)&0xf)));


		for(j=0;j<SPRWI;j++)
		{

			CURPIX=PDEC(bitoper.Read(bpp),&LAMV);

			if(!pdec.Transparent)
			{
int sss=VDX1616;
//if ((CURPIX>35000&&CURPIX<37000)||(CURPIX>38000&&CURPIX<45000)||(CURPIX>60000&&CURPIX<70000)) sss-=1;//w 18 
					if(TexelDraw_Scale(CURPIX, LAMV, xcur>>16, ycur>>16, (xcur+HDX1616+sss)>>16, (ycur+HDY1616+VDY1616)>>16))break;

			}
			xcur+=HDX1616;
			ycur+=HDY1616;

		}
		PDATA+=(offset+2)<<2;

	}
}
else
{

        SPRWI-=((PRE0>>24)&0xf);
	for(i=0;i<SPRHI;i++)
	{
		bitoper.AttachBuffer(PDATA);
		BITCALC=((offset+2)<<2)<<5;

		xcur=xvert;
		ycur=yvert;
		hdx=HDX1616;
		hdy=HDY1616;

		xvert+=VDX1616;
		yvert+=VDY1616;
		HDX1616+=HDDX1616;
		HDY1616+=HDDY1616;

                bitoper.Skip(bpp*(((PRE0>>24)&0xf)));


		xdown=xvert;
		ydown=yvert;

		for(j=0;j<SPRWI;j++)
		{
			CURPIX=PDEC(bitoper.Read(bpp),&LAMV);
if(!pdec.Transparent)
			{
						if(TexelDraw_Arbitrary(CURPIX, LAMV, xcur, ycur, xcur+hdx, ycur+hdy, xdown+HDX1616, ydown+HDY1616, xdown, ydown))break;
						if(speedfixes<1||(speedfixes>=0&&speedfixes<200001)){
							if (CURPIX>30000&&CURPIX<40000)speedfixes=0;
							else speedfixes=-100000;}
			}
			xcur+=hdx;
			ycur+=hdy;
			xdown+=HDX1616;
			ydown+=HDY1616;
		}
		PDATA+=(((offset+2)<<2)/*scipstr*/);


	}
}

        XPOS1616=xcur;
        XPOS=XPOS1616/65536.0;
        //YPOS1616=ycur;
}

void  DrawLRCel_New()
{//doom fix
	sf=100000;
	int i,j,xcur,ycur,xvert,yvert,xdown,ydown,hdx,hdy;
 	unsigned short CURPIX,LAMV;
	unsigned int pixel;
 	unsigned int framePixel;

	bpp=BPP[PRE0&PRE0_BPP_MASK];
	offsetl=2;	if(bpp < 8)	offsetl=1;
	pixcount=0;
	offset=(offsetl==1)?((PRE1&PRE1_WOFFSET8_MASK)>>PRE1_WOFFSET8_SHIFT):((PRE1&PRE1_WOFFSET10_MASK)>>PRE1_WOFFSET10_SHIFT);
	offset+=2;

	SPRWI=1+(PRE1&PRE1_TLHPCNT_MASK);
	SPRHI=(((PRE0&PRE0_VCNT_MASK)>>PRE0_VCNT_SHIFT)<<1)+2;

	if(TestInitVisual(0))return;
	xvert=XPOS1616;
	yvert=YPOS1616;

if(TEXEL_FUN_NUMBER==0)
{
	xvert+=TEXTURE_HI_START*VDX1616;
	yvert+=TEXTURE_HI_START*VDY1616;
	//if(SPRHI>TEXTURE_HI_LIM)SPRHI=TEXTURE_HI_LIM;
	if(SPRWI>TEXTURE_WI_LIM)SPRWI=TEXTURE_WI_LIM;
	for(i=0;i<TEXTURE_HI_LIM;i++)
	{
		xcur=xvert+TEXTURE_WI_START*HDX1616;
		ycur=yvert+TEXTURE_WI_START*HDY1616;
		xvert+=VDX1616;
		yvert+=VDY1616;


		for(j=TEXTURE_WI_START;j<SPRWI;j++)
		{
			CURPIX=PDEC(mreadh((PDATA+XY2OFF(j<<2,i,offset<<2))),&LAMV);

			if(!pdec.Transparent)
			{
					//TexelDraw_Line(CURPIX, LAMV, xcur, ycur, 1);
					
					PlotTexel((xcur>>16)<<2, ycur>>16, CURPIX, LAMV);
			}

			xcur+=HDX1616;
			ycur+=HDY1616;
		}

	}
}
else if(TEXEL_FUN_NUMBER==1)
{


	for(i=0;i<SPRHI;i++)
	{


		xcur=xvert;
		ycur=yvert;
		xvert+=VDX1616;
		yvert+=VDY1616;


		for(j=0;j<SPRWI;j++)
		{

			CURPIX=PDEC(mreadh((PDATA+XY2OFF(j<<2,i,offset<<2))),&LAMV);

			if(!pdec.Transparent)
			{

					if(TexelDraw_Scale(CURPIX, LAMV, xcur>>16, ycur>>16, (xcur+HDX1616+VDX1616)>>16, (ycur+HDY1616+VDY1616)>>16))break;

			}
			xcur+=HDX1616;
			ycur+=HDY1616;


		}

	}
}
else
{
        //return;

        for(i=0;i<SPRHI;i++)
	{


		xcur=xvert;
		ycur=yvert;
		xvert+=VDX1616;
		yvert+=VDY1616;
		xdown=xvert;
		ydown=yvert;
		hdx=HDX1616;
		hdy=HDY1616;
		HDX1616+=HDDX1616;
		HDY1616+=HDDY1616;


		for(j=0;j<SPRWI;j++)
		{
                        CURPIX=PDEC(mreadh((PDATA+XY2OFF(j<<2,i,offset<<2))),&LAMV);

			if(!pdec.Transparent)
			{

					if(TexelDraw_Arbitrary(CURPIX, LAMV, xcur, ycur, xcur+hdx, ycur+hdy, xdown+HDX1616, ydown+HDY1616, xdown, ydown))break;

			}

			xcur+=hdx;
			ycur+=hdy;
			xdown+=HDX1616;
			ydown+=HDY1616;


		}


	}
}

        XPOS1616=xcur;
        XPOS=XPOS1616/65536.0;
        //YPOS1616=ycur;

}



//unsigned int _madam_GetCELCycles()
//{
//	unsigned int val=CELCYCLES; // 1 word = 2 CELCYCLES, 1 hword= 1 CELCYCLE, 8 CELCYCLE=1 CPU SCYCLE
//	CELCYCLES=0;
//	return val;
//}


void _madam_Reset()
{
int i;
for(i=0;i<2048;i++)
	mregs[i]=0;
}


void _madam_SetMapping(unsigned int flag)
{
	MAPPING=flag;
}



#define ROAN_SHIFT 16
#define ROAN_TYPE int

#include <math.h>

__forceinline unsigned int TexelCCWTest(double hdx, double hdy, double vdx, double vdy)
{
        if(((hdx+vdx)*(hdy-vdy)+vdx*vdy-hdx*hdy)<0.0)return CCB_ACCW;
        return CCB_ACW;
}
bool QuardCCWTest(int wdt)
{
 unsigned int tmp;
        if(((CCBFLAGS&CCB_ACCW)) && ((CCBFLAGS&CCB_ACW)))return false;

        tmp=TexelCCWTest(HDX,HDY,VDX,VDY);
        if(tmp!=TexelCCWTest(HDX,HDY,VDX+(HDDX)*(float)wdt,VDY+(HDDY)*(float)wdt))return false;
        if(tmp!=TexelCCWTest(HDX+(HDDX)*SPRHI,HDY+(HDDY)*SPRHI,VDX,VDY))return false;
        if(tmp!=TexelCCWTest(HDX+(HDDX)*SPRHI,HDY+(HDDY)*SPRHI,VDX+(HDDX)*(float)SPRHI*(float)wdt,VDY+(HDDY)*(float)SPRHI*(float)wdt))return false;
        if(tmp==(CCBFLAGS&(CCB_ACCW|CCB_ACW)))
                return true;
        return false;
}

__forceinline int __abs(int iIn)
{
    INT i = (iIn ^ (iIn >> 31)) - (iIn >> 31);
    return i;
}
int TestInitVisual(int packed)
{
	int xpoints[4],ypoints[4];

	if((!(CCBFLAGS&CCB_ACCW)) && (!(CCBFLAGS&CCB_ACW)))return -1;


	if(!packed)
        {
		xpoints[0]=XPOS1616>>16;
		xpoints[1]=(XPOS1616+HDX1616*SPRWI)>>16;
		xpoints[2]=(XPOS1616+VDX1616*SPRHI)>>16;
		xpoints[3]=(XPOS1616+VDX1616*SPRHI+
			(HDX1616+HDDX1616*SPRHI)*SPRWI)>>16;
		if(xpoints[0]<0 && xpoints[1]<0 && xpoints[2]<0 && xpoints[3]<0) return -1;
		if(xpoints[0]>CLIPXVAL && xpoints[1]>CLIPXVAL && xpoints[2]>CLIPXVAL && xpoints[3]>CLIPXVAL) return -1;


		ypoints[0]=YPOS1616>>16;
		ypoints[1]=(YPOS1616+HDY1616*SPRWI)>>16;
		ypoints[2]=(YPOS1616+VDY1616*SPRHI)>>16;
		ypoints[3]=(YPOS1616+VDY1616*SPRHI+
			(HDY1616+HDDY1616*SPRHI)*SPRWI)>>16;
		if(ypoints[0]<0 && ypoints[1]<0 && ypoints[2]<0 && ypoints[3]<0) return -1;
		if(ypoints[0]>CLIPYVAL && ypoints[1]>CLIPYVAL && ypoints[2]>CLIPYVAL && ypoints[3]>CLIPYVAL) return -1;
	}
        else
        {
                xpoints[0]=XPOS1616>>16;
		xpoints[1]=(XPOS1616+VDX1616*SPRHI)>>16;
                if( xpoints[0]<0 && xpoints[1]<0 && HDX1616<=0 && HDDX1616<=0 ) return -1;
                if(xpoints[0]>CLIPXVAL && xpoints[1]>CLIPXVAL && HDX1616>=0 && HDDX1616>=0 ) return -1;

                ypoints[0]=YPOS1616>>16;
		ypoints[1]=(YPOS1616+VDY1616*SPRHI)>>16;
                if(ypoints[0]<0 && ypoints[1]<0 && HDY1616<=0 && HDDY1616<=0 ) return -1;
                if(ypoints[0]>CLIPYVAL && ypoints[1]>CLIPYVAL && HDY1616>=0 && HDDY1616>=0 ) return -1;
        }

	//*
	if(HDDX1616==0 && HDDY1616==0)
	{
		if(HDX1616==0 && VDY1616==0)
		{
					if((HDY1616<0 && VDX1616>0)||(HDY1616>0 && VDX1616<0))
					{
						if((CCBFLAGS&CCB_ACW))
						{
							if(__abs(HDY1616)==0x10000 && __abs(VDX1616)==0x10000 && !((YPOS1616|XPOS1616)&0xffff))
							{
								return Init_Line_Map();
								//return 0;
							}
							else
							{
								Init_Scale_Map();
								return 0;
							}
						}
					}
					else
					{
						if((CCBFLAGS&CCB_ACCW))
						{
							if(__abs(HDY1616)==0x10000 && __abs(VDX1616)==0x10000 && !((YPOS1616|XPOS1616)&0xffff))
							{
								return Init_Line_Map();
								//return 0;
							}
							else
							{
								Init_Scale_Map();
								return 0;
							}
						}

					}
					return -1;


		}
		else if(HDY1616==0 && VDX1616==0)
		{

					if((HDX1616<0 && VDY1616>0)||(HDX1616>0 && VDY1616<0))
					{
						if((CCBFLAGS&CCB_ACCW))
						{
							if(__abs(HDX1616)==0x10000 &&	__abs(VDY1616)==0x10000 && !((YPOS1616|XPOS1616)&0xffff))
							{
								return Init_Line_Map();
								//return 0;
							}
							else
							{
								Init_Scale_Map();
								return 0;
							}
						}
					}
					else
					{
						if((CCBFLAGS&CCB_ACW))
						{
							if(__abs(HDX1616)==0x10000 &&	__abs(VDY1616)==0x10000 && !((YPOS1616|XPOS1616)&0xffff))
							{
								return Init_Line_Map();
								//return 0;
							}
							else
							{
								Init_Scale_Map();
								return 0;
							}
						}
					}
					return -1;


		}
	} //*/

        if(QuardCCWTest((!packed)?SPRWI:2048))return -1;
	Init_Arbitrary_Map();


	return 0;

}

int Init_Line_Map()
{
	TEXEL_FUN_NUMBER=0;
	TEXTURE_WI_START=0;
	TEXTURE_HI_START=0;
	TEXTURE_HI_LIM=SPRHI;
	if(HDX1616<0)
		XPOS1616-=0x8000;
	else if(VDX1616<0)
		XPOS1616-=0x8000;

	if(HDY1616<0)
		YPOS1616-=0x8000;
	else if(VDY1616<0)
		YPOS1616-=0x8000;

	if(VDX1616<0)
	{
		if(((XPOS1616)-((SPRHI-1)<<16)>>16)<0)
			TEXTURE_HI_LIM=(XPOS1616>>16)+1;
		if(TEXTURE_HI_LIM>SPRHI)TEXTURE_HI_LIM=SPRHI;
	}
	else if(VDX1616>0)
	{
		if(((XPOS1616+(SPRHI<<16))>>16)>CLIPXVAL)
			TEXTURE_HI_LIM=CLIPXVAL-(XPOS1616>>16)+1;
	}
	if(VDY1616<0)
	{
		if(((YPOS1616)-((SPRHI-1)<<16)>>16)<0)
			TEXTURE_HI_LIM=(YPOS1616>>16)+1;
		if(TEXTURE_HI_LIM>SPRHI)TEXTURE_HI_LIM=SPRHI;
	}
	else if(VDY1616>0)
	{
		if(((YPOS1616+(SPRHI<<16))>>16)>CLIPYVAL)
			TEXTURE_HI_LIM=CLIPYVAL-(YPOS1616>>16)+1;
	}

	if(HDX1616<0)
		TEXTURE_WI_LIM=(XPOS1616>>16)+1;
	else if(HDX1616>0)
		TEXTURE_WI_LIM=CLIPXVAL-(XPOS1616>>16)+1;

	if(HDY1616<0)
		TEXTURE_WI_LIM=(YPOS1616>>16)+1;
	else if(HDY1616>0)
		TEXTURE_WI_LIM=CLIPYVAL-(YPOS1616>>16)+1;


	if(XPOS1616<0)
	{
		if(HDX1616<0)return -1;
		else if(HDX1616>0)
			TEXTURE_WI_START=-(XPOS1616>>16);

		if(VDX1616<0)return -1;
		else if(VDX1616>0)
			TEXTURE_HI_START=-(XPOS1616>>16);
	}
	else if((XPOS1616>>16)>CLIPXVAL)
	{
		if(HDX1616>0)return -1;
		else if(HDX1616<0)
			TEXTURE_WI_START=(XPOS1616>>16)-CLIPXVAL;

		if(VDX1616>0)return -1;
		else if(VDX1616<0)
			TEXTURE_HI_START=(XPOS1616>>16)-CLIPXVAL;

	}
	if(YPOS1616<0)
	{
		if(HDY1616<0)return -1;
		else if(HDY1616>0)
			TEXTURE_WI_START=-(YPOS1616>>16);

		if(VDY1616<0)return -1;
		else if(VDY1616>0)
			TEXTURE_HI_START=-(YPOS1616>>16);
	}
	else if((YPOS1616>>16)>CLIPYVAL)
	{
		if(HDY1616>0)return -1;
		else if(HDY1616<0)
			TEXTURE_WI_START=(YPOS1616>>16)-CLIPYVAL;

		if(VDY1616>0)return -1;
		else if(VDY1616<0)
			TEXTURE_HI_START=(YPOS1616>>16)-CLIPYVAL;
	}
        //if(TEXTURE_WI_START<((PRE0>>24)&0xf))
        //        TEXTURE_WI_START=((PRE0>>24)&0xf);
	//TEXTURE_WI_START+=(PRE0>>24)&0xf;
	//if(TEXTURE_WI_START<0)TEXTURE_WI_START=0;
	//if(TEXTURE_HI_START<0)TEXTURE_HI_START=0;
	//if(TEXTURE_HI_LIM>SPRHI)TEXTURE_HI_LIM=SPRHI;
	if(TEXTURE_WI_LIM<=0)return -1;
	return 0;
}

void Init_Scale_Map()
{
 int deltax,deltay;
	TEXEL_FUN_NUMBER=1;
	if(HDX1616<0)
		XPOS1616-=0x8000;
	else if(VDX1616<0)
		XPOS1616-=0x8000;

	if(HDY1616<0)
		YPOS1616-=0x8000;
	else if(VDY1616<0)
		YPOS1616-=0x8000;

	deltax=HDX1616+VDX1616;
	deltay=HDY1616+VDY1616;
	if(deltax<0)TEXEL_INCX=-1;
	else TEXEL_INCX=1;
	if(deltay<0)TEXEL_INCY=-1;
	else TEXEL_INCY=1;

	TEXEL_INCX<<=2;

	TEXTURE_WI_START=0;
	TEXTURE_HI_START=0;
}

void Init_Arbitrary_Map()
{
	TEXEL_FUN_NUMBER=2;
	TEXTURE_WI_START=0;
	TEXTURE_HI_START=0;
}

int  TexelDraw_Line(unsigned short CURPIX, unsigned short LAMV, int xcur, int ycur, int cnt)
{
	int i=0;
	unsigned int pixel;
	unsigned int curr=0xffffffff, next;
        xcur>>=16;
        ycur>>=16;
 
	for(i=0;i<cnt;i++,xcur+=(HDX1616>>16),ycur+=(HDY1616>>16))
	{
                next=mreadh((PIXSOURCE+XY2OFF((xcur)<<2,ycur,RMOD)));
                if(next!=curr){curr=next;pixel=PPROC(CURPIX,next,LAMV);}
                //pixel=PPROC(CURPIX,mreadh((PIXSOURCE+XY2OFF((xcur>>16)<<2,ycur>>16,RMOD))),LAMV);
                mwriteh((FBTARGET+XY2OFF((xcur)<<2,ycur,WMOD)),pixel);
	}
 return 0;
}


__inline uint16 readPIX(uint32 src, int i, int j)
{
        src+=XY2OFF(((j)<<2),(i),WMOD);
        return *((uint16*)&Mem[src]);
}

__inline void writePIX(uint32 src, int i, int j, uint16 pix)
{
        src+=XY2OFF(((j)<<2),(i),WMOD);
                *((uint16*)&Mem[src])=pix;
}


int  TexelDraw_Scale(unsigned short CURPIX, unsigned short LAMV, int xcur, int ycur, int deltax, int deltay)
{
	int i,j;
	unsigned int pixel;
	unsigned int curr=-1;
						if((HDX1616<0) && (deltax)<0 && xcur<0)
						{
							return -1;
						}
						else if((HDY1616<0) && (deltay)<0 && ycur<0 )
						{
							return -1;
						}
						else if((HDX1616>0) && (deltax)>(CLIPXVAL) && (xcur)>(CLIPXVAL))
						{
							return -1;
						}
						else if((HDY1616>0) && ((deltay))>(CLIPYVAL) && (ycur)>(CLIPYVAL))
						{
							return -1;
						}
						else
						{
							if((((int)xcur))==(((int)deltax)))return 0;

							for(i=((int)ycur);i!=(((int)deltay));i+=TEXEL_INCY)
								for(j=(((int)xcur))<<2;j!=((((int)deltax))<<2);j+=TEXEL_INCX)
									if((TESTCLIP((j<<14),(i<<16))))
									{
										pixel=PPROC(CURPIX,mreadh((PIXSOURCE+XY2OFF(j,i,RMOD))),LAMV);
										//next=mreadh((PIXSOURCE+XY2OFF(j,i,RMOD)));
										//if(next!=curr){curr=next;pixel=PPROC(CURPIX,next,LAMV);}
										mwriteh((FBTARGET+XY2OFF(j,i,WMOD)),pixel);

									}
						}
						return 0;
}


__forceinline int TexelCCWTestSmp(int hdx, int hdy, int vdx, int vdy)
{
        if(((hdx+vdx)*(hdy-vdy)+vdx*vdy-hdx*hdy)<0)return CCB_ACCW;
        return CCB_ACW;
}

int  TexelDraw_Arbitrary(unsigned short CURPIX, unsigned short LAMV, int xA, int yA, int xB, int yB, int xC, int yC, int xD, int yD)
{
	int miny, maxy, i, xpoints[4], j, maxyt, maxxt, maxx;
	int updowns[4],cnt_cross, jtmp;
	unsigned int pixel;
	unsigned int curr=-1, next;
                                         xA>>=(16);
                                         xB>>=(16);
                                         xC>>=(16);
                                         xD>>=(16);
                                         yA>>=(16);
                                         yB>>=(16);
                                         yC>>=(16);
                                         yD>>=(16);

                                         if((xA)==(xB) && (xB)==(xC) && (xC)==(xD)) return 0;

                                         maxxt=(CLIPXVAL+1);
                                         maxyt=(CLIPYVAL+1);

                                         if(HDX1616<0 && HDDX1616<0)
                                         {
	                                        if((xA<0) && (xB<0) && (xC<0) && (xD<0))
					                return -1;
					 }
                                         if(HDX1616>0 && HDDX1616>0)
                                         {
					        if(((xA)>=maxxt) && ((xB)>=maxxt) && ((xC)>=maxxt) && ((xD)>=maxxt))
                                                        return -1;
                                         }
                                         if(HDY1616<0 && HDDY1616<0)
                                         {
					        if((yA<0) && (yB<0) && (yC<0) && (yD<0))
                                                        return -1;
					 }
                                         if(HDY1616>0 && HDDY1616>0)
                                         {
					        if(((yA)>=maxyt) && ((yB)>=maxyt) && ((yC)>=maxyt) && ((yD)>=maxyt))
					 	        return -1;
                                         }

					 {

						miny=maxy=yA;
						if(miny>yB)miny=yB;
						if(miny>yC)miny=yC;
						if(miny>yD)miny=yD;
						if(maxy<yB)maxy=yB;
						if(maxy<yC)maxy=yC;
						if(maxy<yD)maxy=yD;

						i=(miny);
						if(i<0)i=0;
                                                if(maxy<maxyt)maxyt=maxy;


						for(;i<maxyt;i++)
						{

							cnt_cross=0;
							if(i<(yB) && i>=(yA))
							{
									xpoints[cnt_cross]=(int)((((xB-xA)*(i-yA))/(yB-yA)+xA));
									updowns[cnt_cross++]=1;
							}
							else if(i>=(yB) && i<(yA))
							{
									xpoints[cnt_cross]=(int)((((xA-xB)*(i-yB))/(yA-yB)+xB));
									updowns[cnt_cross++]=0;
							}

							if(i<(yC) && i>=(yB))
							{
									xpoints[cnt_cross]=(int)((((xC-xB)*(i-yB))/(yC-yB)+xB));
									updowns[cnt_cross++]=1;
							}
							else if(i>=(yC) && i<(yB))
							{
									xpoints[cnt_cross]=(int)((((xB-xC)*(i-yC))/(yB-yC)+xC));
									updowns[cnt_cross++]=0;
							}

							if(i<(yD) && i>=(yC))
							{
									xpoints[cnt_cross]=(int)((((xD-xC)*(i-yC))/(yD-yC)+xC));
									updowns[cnt_cross++]=1;
							}
							else if(i>=(yD) && i<(yC))
							{
									xpoints[cnt_cross]=(int)((((xC-xD)*(i-yD))/(yC-yD)+xD));
									updowns[cnt_cross++]=0;
							}

							if(cnt_cross&1)
							{
								if(i<(yA) && i>=(yD))
								{
										xpoints[cnt_cross]=(int)((((xA-xD)*(i-yD))/(yA-yD)+xD));
										updowns[cnt_cross]=1;
								}
								else if(i>=(yA) && i<(yD))
								{
										xpoints[cnt_cross]=(int)((((xD-xA)*(i-yA))/(yD-yA)+xA));
										updowns[cnt_cross]=0;
								}
							}

							if(cnt_cross!=0)
							{

								if(xpoints[0]>xpoints[1])
								{
									xpoints[1]+=xpoints[0];
									xpoints[0]=xpoints[1]-xpoints[0];
									xpoints[1]=xpoints[1]-xpoints[0];

									jtmp=updowns[0];
									updowns[0]=updowns[1];
									updowns[1]=jtmp;
								}
								if(cnt_cross>2)
								{
			/*			for(int i=0; i<2; i++)
           {         
           if(xpoints[i]>xpoints[i+1])  
           {
           xpoints[i+1]+=xpoints[i];
		   xpoints[i]=xpoints[i+1]-xpoints[i];
		   xpoints[i+1]=xpoints[i+1]-xpoints[i];
           jtmp=updowns[i];
		   updowns[i]=updowns[i+1];
           updowns[i+1]=jtmp;
           }    
           }*/
						/*			if(xpoints[1]>xpoints[2])
									{

										xpoints[1]+=xpoints[2];
										xpoints[2]=xpoints[1]-xpoints[2];
										xpoints[1]=xpoints[1]-xpoints[2];

										jtmp=updowns[1];
										updowns[1]=updowns[2];
										updowns[2]=jtmp;

										if(xpoints[0]>xpoints[1])
										{
											xpoints[1]+=xpoints[0];
											xpoints[0]=xpoints[1]-xpoints[0];
											xpoints[1]=xpoints[1]-xpoints[0];

											jtmp=updowns[0];
											updowns[0]=updowns[1];
											updowns[1]=jtmp;
										}
									}
									if(xpoints[2]>xpoints[3])
									{
										xpoints[3]+=xpoints[2];
										xpoints[2]=xpoints[3]-xpoints[2];
										xpoints[3]=xpoints[3]-xpoints[2];

										jtmp=updowns[2];
										updowns[2]=updowns[3];
										updowns[3]=jtmp;
										if(xpoints[1]>xpoints[2])
										{
											xpoints[1]+=xpoints[2];
											xpoints[2]=xpoints[1]-xpoints[2];
											xpoints[1]=xpoints[1]-xpoints[2];

											jtmp=updowns[1];
											updowns[1]=updowns[2];
											updowns[2]=jtmp;
											if(xpoints[0]>xpoints[1])
											{
												xpoints[1]+=xpoints[0];
												xpoints[0]=xpoints[1]-xpoints[0];
												xpoints[1]=xpoints[1]-xpoints[0];

												jtmp=updowns[0];
												updowns[0]=updowns[1];
												updowns[1]=jtmp;
											}
										}
									}*/

									if( ((CCBFLAGS&CCB_ACW)&&updowns[2]==0) ||
										((CCBFLAGS&CCB_ACCW)&&updowns[2]==1))
									{
										j=xpoints[2];
										if(j<0)j=0;
										maxx=xpoints[3]+1; /* borde derecho inclusivo (opera 3590): evita costura de 1px */
										if(maxx>maxxt)maxx=maxxt;
                                                                                for(;j<maxx;j++)
									        {
                                                                                        next=readPIX(PIXSOURCE, i, j);
									                if(next!=curr){curr=next;
                                                                                                pixel=PPROC(CURPIX,next,LAMV);
                                                                                                }
                                                                                        writePIX(FBTARGET, i, j, pixel);
									        }
									}

								}

								if( ((CCBFLAGS&CCB_ACW)&&updowns[0]==0) ||
										((CCBFLAGS&CCB_ACCW)&&updowns[0]==1))
								{
									j=xpoints[0];
									if(j<0)j=0;
									maxx=xpoints[1]+1; /* borde derecho inclusivo (opera 3620): evita costura de 1px */
									if(maxx>maxxt)maxx=maxxt;

                                                                        for(;j<maxx;j++)
									{
                                                                                next=readPIX(PIXSOURCE, i, j);
									        if(next!=curr){curr=next;
                                                                                        pixel=PPROC(CURPIX,next,LAMV);
                                                                                        }
                                                                                writePIX(FBTARGET, i, j, pixel);
									}


								}

							}

						}


					 }
					 return 0;
}




