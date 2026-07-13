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


#ifndef ARM_3DO_HEADER
#define ARM_3DO_HEADER



	int  _arm_Execute();
	void _arm_Reset();
	void _arm_Destroy();
	unsigned char * _arm_Init();
	void _arm_SetSWIHLE(int on);

	//for mas
	void  _mem_write8(unsigned int addr, unsigned char val);
        void  _mem_write16(unsigned int addr, unsigned short val);
        void  _mem_write32(unsigned int addr, unsigned int val);
        unsigned char  _mem_read8(unsigned int addr);
        unsigned short  _mem_read16(unsigned int addr);
        unsigned int  _mem_read32(unsigned int addr);

	void  WriteIO(unsigned int addr, unsigned int val);
	unsigned int  ReadIO(unsigned int addr);
	void  SelectROM(int n);

        unsigned int _arm_SaveSize();
        void _arm_Save(void *buff);
        void _arm_Load(void *buff);
		
		void armIdleCycles(int cycles);
#endif
