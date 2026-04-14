#include <xtl.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "psxcommon.h"
#include "cdriso.h"
#include "r3000a.h"
#include "gui.h"

//char * game = "game:\\Bloody Roar II (USA)\\Bloody Roar II (USA).cue";
//char * game = "game:\\Tekken 3 (USA)\\Tekken 3 (USA) (Track 1).bin";
char * game = "game:\\Tenchu 2.bin";
//char * game = "game:\\Bushido Blade [U] [SCUS-94180]\\bushido_blade.bin";
//char * game = "game:\\Soul Blade (USA) (v1.0)\\Soul Blade (USA) (v1.0).bin";
//char * game = "game:\\Castlevanina - SOTN.bin";
//char * game = "game:\\BH2.bin";
//char * game = "game:\\mgz.bin.Z";
//char * game = "game:\\Final Fantasy VII (USA) (Disc 1)\\Final Fantasy VII (USA) (Disc 1).bin";
//char * game = "game:\\Legend of Mana (USA)\\Legend of Mana (USA).bin";
//char * game = "game:\\Chrono Cross (USA) (Disc 1)\\Chrono Cross (USA) (Disc 1).bin";
//char * game = "game:\\Castlevania Chronicles (USA) (v1.1)\\Castlevania Chronicles (USA) (v1.1).bin";

//char * game = "game:\\Final Fantasy VII (F) (Disc 1)\\Final Fantasy VII (F) (Disc 1).bin";

//extern "C" void gpuDmaThreadInit();

extern void InitD3D();

void PausePcsx() {

   Config.CpuRunning = 0;
}

void ResumePcsx() {

    Config.CpuRunning = 1;
    // Restart emulation ...
	psxCpu->Execute();
}

void ResetPcsx() {

	Config.CpuRunning = 1;
    SysReset();
	ResumePcsx();
}

void ShutdownPcsx() {
	Config.CpuRunning = 0;
	SysClose();
}


void RunPcsx(char * game) {
	// __SetHWThreadPriorityHigh();

	int res, ret;
	XMemSet(&Config, 0, sizeof(PcsxConfig));
	
	Config.Cpu = CPU_INTERPRETER;

	strcpy(Config.Bios, "SCPH1001.BIN"); // Use actual BIOS
	//strcpy(Config.Bios, "HLE"); // Use HLE
	strcpy(Config.BiosDir, "game:\\BIOS");
	strcpy(Config.Mcd1,"game:\\BIOS\\Memcard1.mcd");
	strcpy(Config.Mcd2,"game:\\BIOS\\Memcard2.mcd");

	Config.PsxOut = 1;
	Config.HLE = 0;
	Config.Xa = 0;  //XA enabled
	Config.Cdda = 0;
	Config.PsxAuto = 0; //Autodetect
	// Config.SlowBoot = 1;
	

	//gpuDmaThreadInit();
	//gpuThreadEnable(false);


	//cdrThreadInit();	
	cdrIsoInit();
	SetIsoFile(game);

	if (SysInit() == -1) 
	{
		printf("SysInit() Error!\n");
		return;
	}

	GPU_clearDynarec(clearDynarec);

	ret = CDR_open();
	if (ret < 0) { SysMessage (_("Error Opening CDR Plugin")); return; }
	ret = GPU_open(NULL);
	if (ret < 0) { SysMessage (_("Error Opening GPU Plugin (%d)"), ret); return; }
	ret = SPU_open(NULL);
	if (ret < 0) { SysMessage (_("Error Opening SPU Plugin (%d)"), ret); return; }
	ret = PAD1_open(NULL);
	if (ret < 0) { SysMessage (_("Error Opening PAD1 Plugin (%d)"), ret); return; }

	CDR_init();
	GPU_init();
	SPU_init();
	PAD1_init(1);
	PAD2_init(2);
	
	SysReset();

	SysPrintf("CheckCdrom\r\n");
	res = CheckCdrom();
	if(res)
		SysPrintf("CheckCdrom: %08x\r\n",res);
	res=LoadCdrom();
	if(res)
		SysPrintf("LoadCdrom: %08x\r\n",res);

	SysPrintf("Execute\r\n");
	// SysReset();

	psxCpu->Execute();
	
}

#ifdef NO_GUI
int main() {
#else
int __main() {
#endif
	InitD3D();
	
	RunPcsx(game);

	return 0;
}