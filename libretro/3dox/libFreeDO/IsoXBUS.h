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


#ifndef	ISO_3DO_HEADER
#define ISO_3DO_HEADER 0x00000001

#define XBP_INIT		0	//инициализация плагина, возвращает версию плагина
#define XBP_RESET		1	//сброс с параметром (например путь к образу)
#define XBP_SET_COMMAND	2	//XBUS
#define XBP_FIQ			3	//проверка прерывания от устройства
#define XBP_SET_DATA	4	//XBUS
#define XBP_GET_DATA	5	//XBUS
#define XBP_GET_STATUS	6	//XBUS
#define XBP_SET_POLL	7	//XBUS
#define XBP_GET_POLL	8	//XBUS
#define XBP_SELECT		9   //информирует о том что устройство выбрано оперой - не обязательно к реализации
#define XBP_RESERV		10  //резервный порт (чтение из устройства)
#define XBP_DESTROY		11  //деинициализация плагина

#define XBP_GET_SAVESIZE	19	//для сэйвов - со стороны эмулятора
#define XBP_GET_SAVEDATA	20
#define XBP_SET_SAVEDATA	21

#ifdef XBUS_EXPORTS
#define XBUS_API __declspec(dllexport)
#else
#define XBUS_API __declspec(dllimport)
#endif

/*extern "C"
{
XBUS_API void* _xbplug_MainDevice(int proc, void* data);

#ifdef XBUS_EXPORTS
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 );
#endif
};

#ifndef XBUS_EXPORTS
#pragma comment(lib, "IsoXBUS.lib")
#endif*/

#endif
