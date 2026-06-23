#include <xmedia2.h>
#include "RomList.h"
#include "burner.h"
#include "Splash.h"
#include "version.h"
#include "neocdlist.h"
#include "WorkerThread.h"

HXUIOBJ hPreviewVideo;

extern BOOL IsCurrentlyInGame;

extern IDirect3DDevice9 *pDevice;
extern wchar_t msg[1024];
extern int ArcadeJoystick;
extern int bStartInMVSSlotMode;
extern void simpleReinitScrn(const bool& reinitVid);
char InGamePreview[MAX_PATH];

int nListItems = 0;
int nLastRom = 0;
int nLastFilter = 0;
int HideChildren = 0;
int ThreeOrFourPlayerOnly = 0;

wchar_t DeviceText[60];
extern bool exitGame;
 

map<int, int> m_HardwareFilterMap;
map<int, std::string> m_HardwareFilterDesc;

bool bNeoCDListScanSub			= false;
bool bNeoCDListScanOnlyISO		= false;
TCHAR szNeoCDCoverDir[MAX_PATH] = _T("GAME:\\previews\\neocdz\\");
TCHAR szNeoCDGamesDir[MAX_PATH] = _T("GAME:\\ROMs\\neocdz\\");
TCHAR szVideoPreviewPath[MAX_PATH] = _T("GAME:\\videos\\");

int nLoadMenuPrevShowX			= 0;
int nLoadMenuShowX				= 0;
int nLoadMenuBoardTypeFilter	= 0;
int nLoadMenuGenreFilter		= 0;
int nLoadMenuFamilyFilter		= 0;

struct GAMELIST {
	unsigned int nID;
	bool	bFoundCUE;
	TCHAR	szPathCUE[256];
	TCHAR	szPath[256];
	TCHAR	szISOFile[256];
	int		nAudioTracks;
	TCHAR	szTracks[99][256];
	TCHAR	szShortName[100];
	TCHAR	szPublisher[100];
};

struct NODEINFO {
	int nBurnDrvNo;
	bool bIsParent;	
	TCHAR pszDisplayName[256];
	TCHAR pszROMName[256];
};

static NODEINFO* nBurnDrv;
static unsigned int nTmpDrvCount;
static int nShowMVSCartsOnly	= 0;
static int nSlotLoadMode = 0;
static int SelListMake();

GAMELIST ngcd_list[100];
wchar_t szRomsAvailableInfo[128];

std::string ReplaceCharInString(const std::string & source, char charToReplace, const std::string replaceString);

#define ALL 0
#define CPS1 1
#define CPS2 2
#define CPS3 3
#define NEOGEO 4
#define TAITO 5
#define SEGA 6
#define PGM 7
#define PSYKIO 8
#define KONAMI 9
#define KANEKO 10
#define CAVE 11
#define TOAPLAN 12
#define IREM 13
#define DATAEAST 14
#define GALAXIAN 15
#define PACMAN 16
#define TECHNOS 17
#define MISCPRE90 18
#define MISCPOST90 19

#define NEOGEO_CD 20
#define SEGAMD 21
#define PCE 22
#define SNES 23
#define SETA 24
//#define SMS 25

static int CapcomMiscValue		= HARDWARE_PREFIX_CAPCOM_MISC >> 24;
static int MASKCAPMISC			= 1 << CapcomMiscValue;
static int CaveValue			= HARDWARE_PREFIX_CAVE >> 24;
static int MASKCAVE				= 1 << CaveValue;
static int CpsValue				= HARDWARE_PREFIX_CAPCOM >> 24;
static int MASKCPS				= 1 << CpsValue;
static int Cps2Value			= HARDWARE_PREFIX_CPS2 >> 24;
static int MASKCPS2				= 1 << Cps2Value;
static int Cps3Value			= HARDWARE_PREFIX_CPS3 >> 24;
static int MASKCPS3				= 1 << Cps3Value;
static int DataeastValue		= HARDWARE_PREFIX_DATAEAST >> 24;
static int MASKDATAEAST			= 1 << DataeastValue;
static int GalaxianValue		= HARDWARE_PREFIX_GALAXIAN >> 24;
static int MASKGALAXIAN			= 1 << GalaxianValue;
static int IremValue			= HARDWARE_PREFIX_IREM >> 24;
static int MASKIREM				= 1 << IremValue;
static int KanekoValue			= HARDWARE_PREFIX_KANEKO >> 24;
static int MASKKANEKO			= 1 << KanekoValue;
static int KonamiValue			= HARDWARE_PREFIX_KONAMI >> 24;
static int MASKKONAMI			= 1 << KonamiValue;
static int NeogeoValue			= HARDWARE_PREFIX_SNK >> 24;
static int MASKNEOGEO			= 1 << NeogeoValue;
static int PacmanValue			= HARDWARE_PREFIX_PACMAN >> 24;
static int MASKPACMAN			= 1 << PacmanValue;
static int PgmValue				= HARDWARE_PREFIX_IGS_PGM >> 24;
static int MASKPGM				= 1 << PgmValue;
static int PsikyoValue			= HARDWARE_PREFIX_PSIKYO >> 24;
static int MASKPSIKYO			= 1 << PsikyoValue;
static int SegaValue			= HARDWARE_PREFIX_SEGA >> 24;
static int MASKSEGA				= 1 << SegaValue;
static int SetaValue			= HARDWARE_PREFIX_SETA >> 24;
static int MASKSETA				= 1 << SetaValue;
static int TaitoValue			= HARDWARE_PREFIX_TAITO >> 24;
static int MASKTAITO			= 1 << TaitoValue;
static int TechnosValue			= HARDWARE_PREFIX_TECHNOS >> 24;
static int MASKTECHNOS			= 1 << TechnosValue;
static int ToaplanValue			= HARDWARE_PREFIX_TOAPLAN >> 24;
static int MASKTOAPLAN			= 1 << ToaplanValue;
static int MiscPre90sValue		= HARDWARE_PREFIX_MISC_PRE90S >> 24;
static int MASKMISCPRE90S		= 1 << MiscPre90sValue;
static int MiscPost90sValue		= HARDWARE_PREFIX_MISC_POST90S >> 24;
static int MASKMISCPOST90S		= 1 << MiscPost90sValue;
static int MegadriveValue		= HARDWARE_PREFIX_SEGA_MEGADRIVE >> 24;
static int MASKMEGADRIVE		= 1 << MegadriveValue;
static int PCEngineValue		= HARDWARE_PREFIX_PCENGINE >> 24;
static int MASKPCENGINE			= 1 << PCEngineValue;
static int SnesValue			= HARDWARE_PREFIX_NINTENDO_SNES >> 24;
static int MASKSNES				= 1 << SnesValue;
//static int SmsValue				= HARDWARE_PREFIX_SEGA_MASTER_SYSTEM >> 24;
//static int MASKSMS				= 1 << SmsValue;
static int MASKALL				= MASKCAPMISC | MASKCAVE | MASKCPS | MASKCPS2 | MASKCPS3 | MASKDATAEAST | MASKGALAXIAN | MASKIREM | MASKKANEKO | MASKKONAMI | MASKNEOGEO | MASKPACMAN | MASKPGM | MASKPSIKYO | MASKSEGA | MASKSETA | MASKTAITO | MASKTECHNOS | MASKTOAPLAN | MASKMISCPRE90S | MASKMISCPOST90S | MASKMEGADRIVE | MASKPCENGINE | MASKSNES /*| MASKSMS*/;


#define AVAILONLY				(1 << 28)
#define AUTOEXPAND				(1 << 29)
#define SHOWSHORT				(1 << 30)
#define ASCIIONLY				(1 << 31)

#define MASKBOARDTYPEGENUINE	(1)
#define MASKFAMILYOTHER			0x10000000

#define MASKALLGENRE			(GBF_HORSHOOT | GBF_VERSHOOT | GBF_SCRFIGHT | GBF_VSFIGHT | GBF_BIOS | GBF_BREAKOUT | GBF_CASINO | GBF_BALLPADDLE | GBF_MAZE | GBF_MINIGAMES | GBF_PINBALL | GBF_PLATFORM | GBF_PUZZLE | GBF_QUIZ | GBF_SPORTSMISC | GBF_SPORTSFOOTBALL | GBF_MISC | GBF_MAHJONG | GBF_RACING | GBF_SHOOT)
#define MASKALLFAMILY			(MASKFAMILYOTHER | FBF_MSLUG | FBF_SF | FBF_KOF | FBF_DSTLK | FBF_FATFURY | FBF_SAMSHO | FBF_19XX | FBF_SONICWI | FBF_PWRINST)
#define MASKALLBOARD			(MASKBOARDTYPEGENUINE | BDF_BOOTLEG | BDF_DEMO | BDF_HACK | BDF_HOMEBREW | BDF_PROTOTYPE)

static int CurrentFilter = 0;
static int CurrentSlot = 0;

wchar_t ucString[256];

HXUIOBJ hRomListScene;
HXUIOBJ hInGameOptionsScene = NULL;

LPCWSTR MultiCharToUniChar(char* mbString)
{
	int len = strlen(mbString) + 1;	
	mbstowcs(ucString, mbString, len);
	return (LPCWSTR)ucString;
}
 

void InRescanRomsFirstFunc(XUIMessage *pMsg, InRescanRomsStruct* pData, char *szPath)
{
    XuiMessage(pMsg,XM_MESSAGE_ON_RESCAN_ROMS);
    _XuiMessageExtra(pMsg,(XUIMessageData*) pData, sizeof(*pData));

	
}

// Compact driver loading module
int BurnerLoadDriver(TCHAR *szDriverName)
{
	int ret = 0;
	unsigned int j;
	
	int nOldDrvSelect = nBurnDrvActive;
	DrvExit();

	for (j = 0; j < nBurnDrvCount; j++) {
		nBurnDrvActive = j;
		if (!_tcscmp(szDriverName, BurnDrvGetText(DRV_NAME)) && (!(BurnDrvGetFlags() & BDF_BOARDROM))) {
			nBurnDrvActive = nOldDrvSelect;
 
			DrvExit();
			DrvInit(j, true);	// Init the game driver
			 
			if (bDrvOkay)
			{
				bAltPause = 0;
				AudSoundPlay();			// Restart sound

				if (bVidAutoSwitchFull) {
					nVidFullscreen = 1;
					POST_INITIALISE_MESSAGE;
				}
			}
			else
			{
				ret = 1;
			}
			break;
		}
	}
	
	return ret;
}


// Handler for the XM_RENDER message
/*HRESULT CRomListScene::OnRender(  XUIMessageRender *pRenderData, BOOL& bHandled )
{

	 

	bHandled = TRUE;
	return S_OK;


}*/

// Handler for the XM_NOTIFY message

HRESULT CRomListScene::OnNotifyPress( HXUIOBJ hObjPressed, 
       BOOL& bHandled )
    {
		int nIndex; 

		WCHAR memStr[256];
		MEMORYSTATUS memStatus;
		GlobalMemoryStatus(  &memStatus );
	
		swprintf( memStr, 
            L"Free Memory: %lu Mb/%lu Mb",
            (memStatus.dwAvailPhys/1024/1024), 
            (memStatus.dwTotalPhys/1024/1024) );
 
		m_FreeMemory.SetText(memStr);
 
        if ( hObjPressed == m_RomList )
        {
			nIndex = m_RomList.GetCurSel();

			if (!nSlotLoadMode)
			{
				exitGame = FALSE;
			
				m_RomList.SetShow(false);
				UpdateConsole("Loading Game....");

				nBurnFPS = 6000;
				nFMInterpolation = 0;

				XuiVideoPause(hPreviewVideo, true);

				if (CurrentFilter == NEOGEO_CD)
				{
					//if(ngcd_list[nIndex].bFoundCUE) {
					//	_tcscpy(CDEmuImage, ngcd_list[nIndex].szPathCUE);
					//} else {
						_tcscpy(CDEmuImage, ngcd_list[nIndex].szPath);
					//}

					//NeoCDList_Clean(); 		 
					if (BurnerLoadDriver(_T("neocdz")) == 0)
					{
						nLastRom = m_RomList.GetCurSel();
						nLastFilter = CurrentFilter;
						MediaInit();
						XuiSceneCreate( L"file://game:/media/FBANext.xzp#src\\Intf\\Skin\\", L"InGameOptions.xur", NULL, &hInGameOptionsScene );
						this->NavigateForward(hInGameOptionsScene);									
						RunMessageLoop(); 
						ConfigAppSave();
				
						m_RomList.SetShow(true);
						m_RomList.SetCurSel(nIndex);
						m_RomList.SetFocus();		
						

						DrvExit();
					}

				}
				else
				{					
					if (BurnerLoadDriver(_T(nBurnDrv[nIndex].pszROMName)) == 0)
					{
						nLastRom = m_RomList.GetCurSel();
						nLastFilter = CurrentFilter;
						MediaInit();
						XuiSceneCreate( L"file://game:/media/FBANext.xzp#src\\Intf\\Skin\\", L"InGameOptions.xur", NULL, &hInGameOptionsScene );
						this->NavigateForward(hInGameOptionsScene);									
						RunMessageLoop(); 
						ConfigAppSave();
				
						m_RomList.SetShow(true);
						m_RomList.SetCurSel(nIndex);					
						m_RomList.SetFocus();		
						
					}
					else
					{
						const WCHAR * button_text = L"OK"; 
						ShowMessageBoxEx(L"XuiMessageBox1",NULL,L"FBANext - Rom Error", msg, 1, (LPCWSTR*)&button_text,NULL,  XUI_MB_CENTER_ON_PARENT, NULL); 

						m_RomList.SetShow(true); 
						m_RomList.SetCurSel(nIndex);
					}
				}
		 			
				 
				if (nVidFullscreen) {
					nVidFullscreen = 0;
					VidExit();
				}

				DrvExit();

				UpdateConsole("");
			
				bHandled = TRUE;
				return S_OK;
			}
			else
			{
				wchar_t szItemText[80];

				HXUIOBJ hSlot1;
				HXUIOBJ hSlot2;
				HXUIOBJ hSlot3;
				HXUIOBJ hSlot4;
				HXUIOBJ hSlot5;
				HXUIOBJ hSlot6;

				XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot1", &hSlot1 );
				XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot2", &hSlot2 );
				XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot3", &hSlot3 );
				XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot4", &hSlot4 );
				XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot5", &hSlot5 );
				XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot6", &hSlot6 );
				
				switch (CurrentSlot)
				{

				case 0:
					nBurnDrvSelect[0] = nBurnDrv[nIndex].nBurnDrvNo;
					nBurnDrvActive = nBurnDrvSelect[0];
					_swprintf(szItemText, L"Slot 1 : %S", BurnDrvGetText(DRV_FULLNAME));					 
					XuiTextElementSetText(hSlot1,szItemText);
					break;
				case 1:
					nBurnDrvSelect[1] = nBurnDrv[nIndex].nBurnDrvNo;
					nBurnDrvActive = nBurnDrvSelect[1];
					_swprintf(szItemText, L"Slot 2 : %S", BurnDrvGetText(DRV_FULLNAME));					 
					XuiTextElementSetText(hSlot2,szItemText);
					break;
				case 2:
					nBurnDrvSelect[2] = nBurnDrv[nIndex].nBurnDrvNo;
					nBurnDrvActive = nBurnDrvSelect[2];
					_swprintf(szItemText, L"Slot 3 : %S", BurnDrvGetText(DRV_FULLNAME));					 
					XuiTextElementSetText(hSlot3,szItemText);
					break;
				case 3:
					nBurnDrvSelect[3] = nBurnDrv[nIndex].nBurnDrvNo;
					nBurnDrvActive = nBurnDrvSelect[3];
					_swprintf(szItemText, L"Slot 4 : %S", BurnDrvGetText(DRV_FULLNAME));					 
					XuiTextElementSetText(hSlot4,szItemText);
					break;
				case 4:
					nBurnDrvSelect[4] = nBurnDrv[nIndex].nBurnDrvNo;
					nBurnDrvActive = nBurnDrvSelect[4];
					_swprintf(szItemText, L"Slot 5 : %S", BurnDrvGetText(DRV_FULLNAME));					 
					XuiTextElementSetText(hSlot5,szItemText);
					break;
				case 5:
					nBurnDrvSelect[5] = nBurnDrv[nIndex].nBurnDrvNo;
					nBurnDrvActive = nBurnDrvSelect[5];
					_swprintf(szItemText, L"Slot 6 : %S", BurnDrvGetText(DRV_FULLNAME));					 
					XuiTextElementSetText(hSlot6,szItemText);
					break;
				}



			}

			bHandled = TRUE;
			return S_OK;
  
        }
		else if (hObjPressed == m_StartMultiSlot)
		{
			if (nSlotLoadMode)
			{
				exitGame = FALSE;
				m_MVSMultiSlotScene.SetShow(false);
				nShowMVSCartsOnly = 0;
				nSlotLoadMode = 0;

				nLoadMenuShowX = nLoadMenuPrevShowX;

				XUIMessage xuiMsg;
				InRescanRomsStruct msgData;
				InRescanRomsFirstFunc( &xuiMsg, &msgData, NULL);
				XuiSendMessage( m_RomList.m_hObj, &xuiMsg );

				m_RomList.SetCurSel(0);	

				DoMVSSlotMode(0);
			
				if (nVidFullscreen) {
					nVidFullscreen = 0;
					VidExit();
				}

				DrvExit();

				UpdateConsole("");
			}

			bHandled = TRUE;
			return S_OK;
		}
		else if (hObjPressed == m_RescanRoms)
		{

			if (!nSlotLoadMode)
			{
				XUIMessage xuiMsg;

				bRescanRoms = true;
				CreateROMInfo(this->m_hObj);
				InRescanRomsStruct msgData;
				InRescanRomsFirstFunc( &xuiMsg, &msgData, NULL);
				XuiSendMessage( m_RomList.m_hObj, &xuiMsg );

 
				m_RomList.SetCurSelVisible(0);
				m_RomList.SetCurSel(0);	
				m_RomList.SetFocus();				


			}
			bHandled = TRUE;
			return S_OK;
		}
		else if (hObjPressed == m_ClearMultiSlot)
		{
			if (nSlotLoadMode)
			{

				HXUIOBJ hSlot1;
				HXUIOBJ hSlot2;
				HXUIOBJ hSlot3;
				HXUIOBJ hSlot4;
				HXUIOBJ hSlot5;
				HXUIOBJ hSlot6;

				XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot1", &hSlot1 );
				XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot2", &hSlot2 );
				XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot3", &hSlot3 );
				XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot4", &hSlot4 );
				XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot5", &hSlot5 );
				XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot6", &hSlot6 );

				XuiTextElementSetText(hSlot1,L"Slot 1 : Empty");
				XuiTextElementSetText(hSlot2,L"Slot 2 : Empty");
				XuiTextElementSetText(hSlot3,L"Slot 3 : Empty");
				XuiTextElementSetText(hSlot4,L"Slot 4 : Empty");
				XuiTextElementSetText(hSlot5,L"Slot 5 : Empty");
				XuiTextElementSetText(hSlot6,L"Slot 6 : Empty");

				for (int i = 0; i < 6; i++) {
					nBurnDrvSelect[i] = ~0U;
				}
			}

			bHandled = TRUE;
			return S_OK;
		}
		else if (hObjPressed == m_MVSMultiSlot)
		{

			if (!nSlotLoadMode)
			{
				m_MVSMultiSlotScene.SetShow(true);
				m_LaunchRom.SetText(L"Add Game to Slot");
				//CurrentFilter = MASKALL;
				nShowMVSCartsOnly = 1;
				nSlotLoadMode = 1;
				nLoadMenuPrevShowX = nLoadMenuShowX;
				nLoadMenuShowX = MASKNEOGEO;
				nLoadMenuShowX |= AVAILONLY;
				
				
				XUIMessage xuiMsg;
				InRescanRomsStruct msgData;
				InRescanRomsFirstFunc( &xuiMsg, &msgData, NULL);
				XuiSendMessage( m_RomList.m_hObj, &xuiMsg );
	
				swprintf_s(DeviceText, L"Current Slot : 1");

				m_Hardware.SetText(DeviceText); 				
				m_RomList.SetCurSelVisible(0);
				m_RomList.SetCurSel(1);	
				m_RomList.SetCurSel(0);	
				m_RomList.SetFocus();				
			}
			else
			{
				m_MVSMultiSlotScene.SetShow(false);
				
				nShowMVSCartsOnly = 0;
				nSlotLoadMode = 0;

				nLoadMenuShowX = nLoadMenuPrevShowX;

				XUIMessage xuiMsg;
				InRescanRomsStruct msgData;
				InRescanRomsFirstFunc( &xuiMsg, &msgData, NULL);
				XuiSendMessage( m_RomList.m_hObj, &xuiMsg );
 
				swprintf_s(DeviceText, L"%S", m_HardwareFilterDesc[CurrentFilter].c_str());
				m_Hardware.SetText(DeviceText);				

				m_Hardware.SetText(DeviceText); 				
				m_RomList.SetCurSelVisible(0);
				m_RomList.SetCurSel(1);	
				m_RomList.SetCurSel(0);	
				m_RomList.SetFocus();	
			}
 

			bHandled = TRUE;
			return S_OK;
		}
		else if (hObjPressed == m_UseArcadeStickSettings)
		{
			if (m_UseArcadeStickSettings.IsChecked())
			{
				ArcadeJoystick = 1;
 
			}
			else
			{
				ArcadeJoystick = 0; 

			}

		}
		else if (hObjPressed == m_HighScore)
		{

			if (m_HighScore.IsChecked())
			{
				EnableHiscores = 1;
			}
			else
			{
				EnableHiscores = 0;
			}

		}
		else if (hObjPressed == m_34PlayerOnly)
		{

			if (m_34PlayerOnly.IsChecked())
			{
				ThreeOrFourPlayerOnly = 1;
			}
			else
			{
				ThreeOrFourPlayerOnly = 0;
			}

			XuiImageElementSetImagePath(m_PreviewImage.m_hObj, L"");

			XUIMessage xuiMsg;
			InRescanRomsStruct msgData;
			InRescanRomsFirstFunc( &xuiMsg, &msgData, NULL);
			XuiSendMessage( m_RomList.m_hObj, &xuiMsg );

			bHandled = TRUE;
			return S_OK;
		}
		else if (hObjPressed == m_HideChildren)
		{
 
			nLoadMenuShowX ^= AUTOEXPAND;
			XuiImageElementSetImagePath(m_PreviewImage.m_hObj, L"");

			XUIMessage xuiMsg;
			InRescanRomsStruct msgData;
			InRescanRomsFirstFunc( &xuiMsg, &msgData, NULL);
			XuiSendMessage( m_RomList.m_hObj, &xuiMsg );

			m_RomList.SetCurSelVisible(0);
			m_RomList.SetCurSel(0);	
			m_RomList.SetFocus();	

			bHandled = TRUE;
			return S_OK;
		}
		else if (hObjPressed == m_PrevDevice || hObjPressed == m_PrevDevice1)
		{
			if (!nSlotLoadMode)
			{
				CurrentFilter--;

				if (CurrentFilter < 0)
				{
					CurrentFilter = 24;
				}
			
				nLoadMenuShowX = m_HardwareFilterMap[CurrentFilter];
				nLoadMenuShowX |= AVAILONLY;

				if (m_HideChildren.IsChecked())
				{
					nLoadMenuShowX |= AUTOEXPAND;
				}

				XUIMessage xuiMsg;
				InRescanRomsStruct msgData;
				InRescanRomsFirstFunc( &xuiMsg, &msgData, NULL);
				XuiSendMessage( m_RomList.m_hObj, &xuiMsg );
	
				swprintf_s(DeviceText, L"%S", m_HardwareFilterDesc[CurrentFilter].c_str());
				m_Hardware.SetText(DeviceText);
				
				m_RomList.SetCurSelVisible(0);				
				m_RomList.SetFocus();	
				m_RomList.SetCurSel(1);	
				m_RomList.SetCurSel(0);	
			}
			else
			{
				CurrentSlot--;

				if (CurrentSlot < 0)
				{
					CurrentSlot = 5;
				}

				swprintf_s(DeviceText, L"Current Slot : %d", CurrentSlot+1);

				m_Hardware.SetText(DeviceText);

			}


			return S_OK;

		}
		else if (hObjPressed == m_NextDevice || hObjPressed == m_NextDevice1)
		{
			if (!nSlotLoadMode)
			{
				CurrentFilter++;

				if (CurrentFilter > 24)
				{
					CurrentFilter = 0;
				}


				//XuiImageElementSetImagePath(m_PreviewImage.m_hObj, L"");
 
				nLoadMenuShowX = m_HardwareFilterMap[CurrentFilter];
				nLoadMenuShowX |= AVAILONLY;

				if (m_HideChildren.IsChecked())
				{
					nLoadMenuShowX |= AUTOEXPAND;
				}

				XUIMessage xuiMsg;
				InRescanRomsStruct msgData;
				InRescanRomsFirstFunc( &xuiMsg, &msgData, NULL);
				XuiSendMessage( m_RomList.m_hObj, &xuiMsg );
	
				swprintf_s(DeviceText, L"%S", m_HardwareFilterDesc[CurrentFilter].c_str());
				m_Hardware.SetText(DeviceText);

				m_RomList.SetCurSelVisible(0);				 
				m_RomList.SetFocus();

				m_RomList.SetCurSel(1);	
				m_RomList.SetCurSel(0);	
			}
			else
			{
				CurrentSlot++;

				if (CurrentSlot > 5)
				{
					CurrentSlot = 0;
				}

				swprintf_s(DeviceText, L"Current Slot : %d", CurrentSlot+1);
				m_Hardware.SetText(DeviceText);
			}

			return S_OK;

		}
	
        bHandled = TRUE;
        return S_OK;
    }

void CRomListScene::DoMVSSlotMode(int nIndex)
{

	if (BurnerLoadDriver(_T("neogeo")) == 0)
	{

		nLastRom = m_RomList.GetCurSel();
		nLastFilter = CurrentFilter;
		MediaInit();
		XuiSceneCreate( L"file://game:/media/FBANext.xzp#src\\Intf\\Skin\\", L"InGameOptions.xur", NULL, &hInGameOptionsScene );
		this->NavigateForward(hInGameOptionsScene);									
		RunMessageLoop(); 
		ConfigAppSave();
				
		m_RomList.SetShow(true);
		m_RomList.SetCurSel(nIndex); 
		m_RomList.SetFocus();				
	}
	else
	{
		const WCHAR * button_text = L"OK"; 
		ShowMessageBoxEx(L"XuiMessageBox1",NULL,L"FBANext - Rom Error", msg, 1, (LPCWSTR*)&button_text,NULL,  XUI_MB_CENTER_ON_PARENT, NULL); 

		m_RomList.SetShow(true); 
		m_RomList.SetCurSel(nIndex);
	}

	UpdateConsole("");

}

    //----------------------------------------------------------------------------------
    // Performs initialization tasks - retreives controls.
    //----------------------------------------------------------------------------------
HRESULT CRomListScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
		
		HXUIOBJ hSlot1;
		HXUIOBJ hSlot2;
		HXUIOBJ hSlot3;
		HXUIOBJ hSlot4;
		HXUIOBJ hSlot5;
		HXUIOBJ hSlot6;
		 
		wchar_t VersionText[60];
		wchar_t HardwareBGImage[60];
		wchar_t szItemText[80];
 
        // Retrieve controls for later use.
        GetChildById( L"XuiAddToFavorites", &m_AddToFavorites );
        GetChildById( L"XuiImage1", &m_SkinImage );
        GetChildById( L"XuiMainMenu", &m_Back );		 
		GetChildById( L"XuiRomList", &m_RomList );
		GetChildById( L"XuiRomPreview", &m_PreviewImage );	
		GetChildById( L"XuiRomTitle", &m_TitleImage );			 
		GetChildById( L"XuiBackVideoRoms", &m_BackVideo );
		GetChildById( L"XuiCurrentDeviceText", &m_DeviceText);

		GetChildById( L"XuiToggleHardwareNext", &m_NextDevice);
		GetChildById( L"XuiToggleHardwarePrev", &m_PrevDevice);
		GetChildById( L"XuiToggleHardwareNext1", &m_NextDevice1);
		GetChildById( L"XuiToggleHardwarePrev1", &m_PrevDevice1);



		GetChildById( L"XuiOptions", &m_ConsoleOutput);

		GetChildById( L"XuiGameInfo", &m_GameInfo);
		GetChildById( L"XuiRomName", &m_RomName);
		GetChildById( L"XuiROMInfo", &m_RomInfo);
		GetChildById( L"XuiReleasedBy", &m_ReleasedBy);
		GetChildById( L"XuiGenre", &m_Genre);
		GetChildById( L"XuiNotes", &m_Notes);

		GetChildById( L"XuiVersion", &m_Version);
		GetChildById( L"XuiHardware", &m_Hardware);
		GetChildById( L"XuiNumberOfRoms", &m_NumberOfGames);
		GetChildById( L"XuiCheckboxHideChildren", &m_HideChildren);
		GetChildById( L"XuiCheckboxShow34PlayerOnly", &m_34PlayerOnly);
		GetChildById( L"XuiArcadeStick", &m_UseArcadeStickSettings);

		GetChildById( L"XuiMultiSlot", &m_MVSMultiSlot);		 
		GetChildById( L"XuiMultiSlotScene", &m_MVSMultiSlotScene);

		GetChildById( L"XuiButtonLaunch", &m_LaunchRom);
		GetChildById( L"XuiMultiSlotClear", &m_ClearMultiSlot);
		GetChildById( L"XuiMultiSlotStart", &m_StartMultiSlot);

		GetChildById( L"XuiRescanRoms", &m_RescanRoms);

		GetChildById( L"XuiFreeMemory", &m_FreeMemory);
		GetChildById( L"XuiHighScores", &m_HighScore);
 
		if (ThreeOrFourPlayerOnly==1)
		{
			m_34PlayerOnly.SetCheck(true);
		}

		if (ArcadeJoystick==1)
		{
			m_UseArcadeStickSettings.SetCheck(true);
		}

		if (EnableHiscores==1)
		{
			m_HighScore.SetCheck(true);
		}
 
 
 
		hRomListScene = this->m_hObj;

		// build the hardware filter map
 
		m_HardwareFilterMap[ALL] = MASKALL;
		m_HardwareFilterMap[CPS1] = MASKCPS;
		m_HardwareFilterMap[CPS2] = MASKCPS2;
		m_HardwareFilterMap[CPS3] = MASKCPS3;
		m_HardwareFilterMap[NEOGEO] = MASKNEOGEO;
		m_HardwareFilterMap[NEOGEO_CD] = HARDWARE_SNK_NEOCD;
		m_HardwareFilterMap[TAITO] = MASKTAITO;
		m_HardwareFilterMap[SEGA] = MASKSEGA;
		m_HardwareFilterMap[PGM] = MASKPGM;
		m_HardwareFilterMap[PSYKIO] = MASKPSIKYO;
		m_HardwareFilterMap[KONAMI] = MASKKONAMI;
		m_HardwareFilterMap[KANEKO] = MASKKANEKO;
		m_HardwareFilterMap[CAVE] = MASKCAVE;
		m_HardwareFilterMap[TOAPLAN] = MASKTOAPLAN;
		m_HardwareFilterMap[SEGAMD] = MASKMEGADRIVE;
		m_HardwareFilterMap[IREM] = MASKIREM;
		m_HardwareFilterMap[DATAEAST] = MASKDATAEAST;
		m_HardwareFilterMap[GALAXIAN] = MASKGALAXIAN;
		m_HardwareFilterMap[PACMAN] = MASKPACMAN;
		m_HardwareFilterMap[TECHNOS] = MASKTECHNOS;
		m_HardwareFilterMap[MISCPRE90] = MASKMISCPRE90S;
		m_HardwareFilterMap[MISCPOST90] = MASKMISCPOST90S;
		m_HardwareFilterMap[PCE] = MASKPCENGINE;
		m_HardwareFilterMap[SNES] = MASKSNES;
		//m_HardwareFilterMap[SMS] = MASKSMS;
		m_HardwareFilterMap[SETA] = MASKSETA;

		m_HardwareFilterDesc[ALL] = "All Hardware";
		m_HardwareFilterDesc[CPS1] = "Capcom CPS1";
		m_HardwareFilterDesc[CPS2] = "Capcom CPS2";
		m_HardwareFilterDesc[CPS3] = "Capcom CPS3";
		m_HardwareFilterDesc[NEOGEO] = "NeoGeo";
		m_HardwareFilterDesc[NEOGEO_CD] = "NeoGeo CD";
		m_HardwareFilterDesc[TAITO] = "Taito";
		m_HardwareFilterDesc[SEGA] = "Sega";
		m_HardwareFilterDesc[PGM] = "IGS PGM";
		m_HardwareFilterDesc[PSYKIO] = "Psykio";
		m_HardwareFilterDesc[KONAMI] = "Konami";
		m_HardwareFilterDesc[KANEKO] = "Kaneko";
		m_HardwareFilterDesc[CAVE] = "Cave";
		m_HardwareFilterDesc[TOAPLAN] = "Toaplan";
		m_HardwareFilterDesc[SEGAMD] = "Sega Megadrive";
		m_HardwareFilterDesc[IREM] = "Irem";
		m_HardwareFilterDesc[DATAEAST] = "Data East";
		m_HardwareFilterDesc[GALAXIAN] = "Galaxian";
		m_HardwareFilterDesc[PACMAN] = "Pacman";
		m_HardwareFilterDesc[TECHNOS] = "Technos";
		m_HardwareFilterDesc[MISCPRE90] = "Pre 90's";
		m_HardwareFilterDesc[MISCPOST90] = "Post 90's";
		m_HardwareFilterDesc[PCE] = "PC Engine";
		m_HardwareFilterDesc[SNES] = "SNES";
		//m_HardwareFilterDesc[SNES] = "SMS";
		m_HardwareFilterDesc[SETA] = "Seta";
 

		swprintf(VersionText,L"%S",szAppBurnVer);
		m_Version.SetText(VersionText);
 
		nLoadMenuShowX |= MASKALL;
		nLoadMenuShowX |= AVAILONLY;

		if (nLoadMenuShowX & AUTOEXPAND)
		{
			m_HideChildren.SetCheck(true);
		}

		CurrentFilter = nLastFilter;
		XuiImageElementSetImagePath(m_PreviewImage.m_hObj, L"");

 		XUIMessage xuiMsg;
		InRescanRomsStruct msgData;
		InRescanRomsFirstFunc( &xuiMsg, &msgData, NULL);
		XuiSendMessage( m_RomList.m_hObj, &xuiMsg );

		swprintf_s(DeviceText, L"Current Hardware : %S", m_HardwareFilterDesc[CurrentFilter].c_str());
		m_DeviceText.SetText(DeviceText);

		//swprintf_s(HardwareBGImage,L"%S",m_HardwareFilterBackGroundMap[CurrentFilter].c_str());			 		 
		//XuiImageElementSetImagePath(m_SkinImage.m_hObj, HardwareBGImage);
 
		m_RomList.SetFocus();	
		m_RomList.SetShow(true);
		m_RomList.SetCurSel(nLastRom);
		m_RomList.SetCurSelVisible(nLastRom);
			

		m_MVSMultiSlotScene.SetShow(false);
 
		XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot1", &hSlot1 );
		XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot2", &hSlot2 );
		XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot3", &hSlot3 );
		XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot4", &hSlot4 );
		XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot5", &hSlot5 );
		XuiElementGetChildById( m_MVSMultiSlotScene.m_hObj, L"XuiSlot6", &hSlot6 );
				 			 
		if (nBurnDrvSelect[0] != -1)
		{
			nBurnDrvActive = nBurnDrvSelect[0];
			_swprintf(szItemText, L"Slot 1 : %S", BurnDrvGetText(DRV_FULLNAME));					 
			XuiTextElementSetText(hSlot1,szItemText);
		}
			 
		if (nBurnDrvSelect[1] != -1)
		{
			nBurnDrvActive = nBurnDrvSelect[1];
			_swprintf(szItemText, L"Slot 2 : %S", BurnDrvGetText(DRV_FULLNAME));					 
			XuiTextElementSetText(hSlot2,szItemText);
		}
		 	 
		if (nBurnDrvSelect[2] != -1)
		{
			nBurnDrvActive = nBurnDrvSelect[2];
			_swprintf(szItemText, L"Slot 3 : %S", BurnDrvGetText(DRV_FULLNAME));					 
			XuiTextElementSetText(hSlot3,szItemText);
		}
		 	 
		if (nBurnDrvSelect[3] != -1)
		{
			nBurnDrvActive = nBurnDrvSelect[3];
			_swprintf(szItemText, L"Slot 4 : %S", BurnDrvGetText(DRV_FULLNAME));					 
			XuiTextElementSetText(hSlot4,szItemText);
		}
		 	 
		if (nBurnDrvSelect[4] != -1)
		{
			nBurnDrvActive = nBurnDrvSelect[4];
			_swprintf(szItemText, L"Slot 5 : %S", BurnDrvGetText(DRV_FULLNAME));					 
			XuiTextElementSetText(hSlot5,szItemText);
		}
		 		 
		if (nBurnDrvSelect[5] != -1)
		{
			nBurnDrvActive = nBurnDrvSelect[5];
			_swprintf(szItemText, L"Slot 6 : %S", BurnDrvGetText(DRV_FULLNAME));					 
			XuiTextElementSetText(hSlot6,szItemText);
		}

		WCHAR memStr[256];
		MEMORYSTATUS memStatus;
		GlobalMemoryStatus(  &memStatus );
	
		swprintf( memStr, 
            L"Free Memory: %lu Mb/%lu Mb",
            (memStatus.dwAvailPhys/1024/1024), 
            (memStatus.dwTotalPhys/1024/1024) );
 
		m_FreeMemory.SetText(memStr);
	 

		UpdateConsole("");

		bHandled = TRUE;
        return S_OK;
    }
	

CRomList::CRomList()
{
	
}

int CRomList::InitRomList()
{

	nBurnDrvActive = 0;	
	RomListOK = false;
 
	nListItems = 0;
	
	return 0;
} 

int CRomList::FreeRomList()
{
 
	return 0;
}
 


int CRomList::AvRoms()
{
 
	return 0;
}
 

 
static int DoExtraFilters()
{
	if (nShowMVSCartsOnly && ((BurnDrvGetHardwareCode() & HARDWARE_PREFIX_CARTRIDGE) != HARDWARE_PREFIX_CARTRIDGE)) return 1;
	
	if ((nLoadMenuBoardTypeFilter & BDF_BOOTLEG)			&& (BurnDrvGetFlags() & BDF_BOOTLEG))				return 1;
	if ((nLoadMenuBoardTypeFilter & BDF_DEMO)				&& (BurnDrvGetFlags() & BDF_DEMO))					return 1;
	if ((nLoadMenuBoardTypeFilter & BDF_HACK)				&& (BurnDrvGetFlags() & BDF_HACK))					return 1;
	if ((nLoadMenuBoardTypeFilter & BDF_HOMEBREW)			&& (BurnDrvGetFlags() & BDF_HOMEBREW))				return 1;
	if ((nLoadMenuBoardTypeFilter & BDF_PROTOTYPE)			&& (BurnDrvGetFlags() & BDF_PROTOTYPE))				return 1;
	
	if ((nLoadMenuBoardTypeFilter & MASKBOARDTYPEGENUINE)	&& (!(BurnDrvGetFlags() & BDF_BOOTLEG)) 
															&& (!(BurnDrvGetFlags() & BDF_DEMO)) 
															&& (!(BurnDrvGetFlags() & BDF_HACK)) 
															&& (!(BurnDrvGetFlags() & BDF_HOMEBREW)) 
															&& (!(BurnDrvGetFlags() & BDF_PROTOTYPE)))	return 1;
	
	if ((nLoadMenuFamilyFilter & FBF_MSLUG)					&& (BurnDrvGetFamilyFlags() & FBF_MSLUG))			return 1;
	if ((nLoadMenuFamilyFilter & FBF_SF)					&& (BurnDrvGetFamilyFlags() & FBF_SF))				return 1;
	if ((nLoadMenuFamilyFilter & FBF_KOF)					&& (BurnDrvGetFamilyFlags() & FBF_KOF))				return 1;
	if ((nLoadMenuFamilyFilter & FBF_DSTLK)					&& (BurnDrvGetFamilyFlags() & FBF_DSTLK))			return 1;
	if ((nLoadMenuFamilyFilter & FBF_FATFURY)				&& (BurnDrvGetFamilyFlags() & FBF_FATFURY))			return 1;
	if ((nLoadMenuFamilyFilter & FBF_SAMSHO)				&& (BurnDrvGetFamilyFlags() & FBF_SAMSHO))			return 1;
	if ((nLoadMenuFamilyFilter & FBF_19XX)					&& (BurnDrvGetFamilyFlags() & FBF_19XX))			return 1;
	if ((nLoadMenuFamilyFilter & FBF_SONICWI)				&& (BurnDrvGetFamilyFlags() & FBF_SONICWI))			return 1;
	if ((nLoadMenuFamilyFilter & FBF_PWRINST)				&& (BurnDrvGetFamilyFlags() & FBF_PWRINST))			return 1;
	
	if ((nLoadMenuFamilyFilter & MASKFAMILYOTHER)			&& (!(BurnDrvGetFamilyFlags() & FBF_MSLUG)) 
															&& (!(BurnDrvGetFamilyFlags() & FBF_SF)) 
															&& (!(BurnDrvGetFamilyFlags() & FBF_KOF)) 
															&& (!(BurnDrvGetFamilyFlags() & FBF_DSTLK)) 
															&& (!(BurnDrvGetFamilyFlags() & FBF_FATFURY)) 
															&& (!(BurnDrvGetFamilyFlags() & FBF_SAMSHO)) 
															&& (!(BurnDrvGetFamilyFlags() & FBF_19XX)) 
															&& (!(BurnDrvGetFamilyFlags() & FBF_SONICWI)) 
															&& (!(BurnDrvGetFamilyFlags() & FBF_PWRINST)))		return 1;
	
	if ((nLoadMenuGenreFilter & GBF_HORSHOOT)				&& (BurnDrvGetGenreFlags() & GBF_HORSHOOT))			return 1;
	if ((nLoadMenuGenreFilter & GBF_VERSHOOT)				&& (BurnDrvGetGenreFlags() & GBF_VERSHOOT))			return 1;
	if ((nLoadMenuGenreFilter & GBF_SCRFIGHT)				&& (BurnDrvGetGenreFlags() & GBF_SCRFIGHT))			return 1;
	if ((nLoadMenuGenreFilter & GBF_VSFIGHT)				&& (BurnDrvGetGenreFlags() & GBF_VSFIGHT))			return 1;
	if ((nLoadMenuGenreFilter & GBF_BIOS)					&& (BurnDrvGetGenreFlags() & GBF_BIOS))				return 1;
	if ((nLoadMenuGenreFilter & GBF_BREAKOUT)				&& (BurnDrvGetGenreFlags() & GBF_BREAKOUT))			return 1;
	if ((nLoadMenuGenreFilter & GBF_CASINO)					&& (BurnDrvGetGenreFlags() & GBF_CASINO))			return 1;
	if ((nLoadMenuGenreFilter & GBF_BALLPADDLE)				&& (BurnDrvGetGenreFlags() & GBF_BALLPADDLE))		return 1;
	if ((nLoadMenuGenreFilter & GBF_MAZE)					&& (BurnDrvGetGenreFlags() & GBF_MAZE))				return 1;
	if ((nLoadMenuGenreFilter & GBF_MINIGAMES)				&& (BurnDrvGetGenreFlags() & GBF_MINIGAMES))		return 1;
	if ((nLoadMenuGenreFilter & GBF_PINBALL)				&& (BurnDrvGetGenreFlags() & GBF_PINBALL))			return 1;
	if ((nLoadMenuGenreFilter & GBF_PLATFORM)				&& (BurnDrvGetGenreFlags() & GBF_PLATFORM))			return 1;
	if ((nLoadMenuGenreFilter & GBF_PUZZLE)					&& (BurnDrvGetGenreFlags() & GBF_PUZZLE))			return 1;
	if ((nLoadMenuGenreFilter & GBF_QUIZ)					&& (BurnDrvGetGenreFlags() & GBF_QUIZ))				return 1;
	if ((nLoadMenuGenreFilter & GBF_SPORTSMISC)				&& (BurnDrvGetGenreFlags() & GBF_SPORTSMISC))		return 1;
	if ((nLoadMenuGenreFilter & GBF_SPORTSFOOTBALL) 		&& (BurnDrvGetGenreFlags() & GBF_SPORTSFOOTBALL))	return 1;
	if ((nLoadMenuGenreFilter & GBF_MISC)					&& (BurnDrvGetGenreFlags() & GBF_MISC))				return 1;
	if ((nLoadMenuGenreFilter & GBF_MAHJONG)				&& (BurnDrvGetGenreFlags() & GBF_MAHJONG))			return 1;
	if ((nLoadMenuGenreFilter & GBF_RACING)					&& (BurnDrvGetGenreFlags() & GBF_RACING))			return 1;
	if ((nLoadMenuGenreFilter & GBF_SHOOT)					&& (BurnDrvGetGenreFlags() & GBF_SHOOT))			return 1;
	
	return 0;
}
 
// Make a tree-view control with all drivers
static int SelListMake()
{
	HXUIOBJ hRomCountText;
	unsigned int i, j;
	unsigned int nMissingDrvCount = 0;
	
	if (nBurnDrv) {
		free(nBurnDrv);
		nBurnDrv = NULL;
	}
	nBurnDrv = (NODEINFO*)malloc(nBurnDrvCount * sizeof(NODEINFO));
	memset(nBurnDrv, 0, nBurnDrvCount * sizeof(NODEINFO));

	nTmpDrvCount = 0;
  

	// Add all the driver names to the list
 
	for (i = 0; i < nBurnDrvCount; i++) {
		 
		nBurnDrvActive = i;																// Switch to driver i

		if (BurnDrvGetFlags() & BDF_BOARDROM) {
			continue;
		}

		if (BurnDrvGetMaxPlayers() < 3 && ThreeOrFourPlayerOnly == 1)
			continue;

		if ((BurnDrvGetFlags() & BDF_CLONE) && (nLoadMenuShowX & AUTOEXPAND))
			continue;
 
		int nHardware = 1 << (BurnDrvGetHardwareCode() >> 24);
		if ((nHardware & nLoadMenuShowX) == 0) {
			continue;
		}
		
		if (DoExtraFilters()) continue;
		
		if(!gameAv[i]) nMissingDrvCount++;

		if (avOk && (nLoadMenuShowX & AVAILONLY) && !gameAv[i])	{						// Skip non-available games if needed
			continue;
		}
 
		nBurnDrv[nTmpDrvCount].nBurnDrvNo = i;
		strcpy(nBurnDrv[nTmpDrvCount].pszROMName,BurnDrvGetTextA(DRV_NAME));
		nBurnDrv[nTmpDrvCount].bIsParent = true;
		strcpy(nBurnDrv[nTmpDrvCount].pszDisplayName,BurnDrvGetText(DRV_ASCIIONLY | DRV_FULLNAME));
		nTmpDrvCount++;
	}

 
	// Update the status info		

	swprintf_s(szRomsAvailableInfo, L"Showing %i of %i sets [%i Unavailable sets]", nTmpDrvCount, nBurnDrvCount - 3, nMissingDrvCount);
 
	return 0;
}

TCHAR szFileName[MAX_PATH];

TCHAR* GetPreviewImage(int nIndex, TCHAR *szPath)
{
	static bool bTryParent;

	TCHAR szBaseName[MAX_PATH];
 
	FILE* fp = NULL;

	nBurnDrvActive = nIndex;

	// Try to load a .PNG preview image
	_sntprintf(szBaseName, sizeof(szBaseName), _T("%s%s"), szPath, BurnDrvGetText(DRV_NAME));
	_stprintf(szFileName, _T("%s.png"), szBaseName);

	return szFileName;
}

TCHAR* GetVideoImage(int nIndex, TCHAR *szPath)
{
	static bool bTryParent;

	TCHAR szBaseName[MAX_PATH];
 
	FILE* fp = NULL;

	nBurnDrvActive = nIndex;

	// Try to load a .wmv preview image
	_sntprintf(szBaseName, sizeof(szBaseName), _T("%s%s"), szPath, BurnDrvGetText(DRV_NAME));
	_stprintf(szFileName, _T("%s.wmv"), szBaseName);

	return szFileName;
}


static void UpdateInfoRelease()
{
	


 
	return;
}

TCHAR* GetGenreInfo()
{
	INT32 nGenre = BurnDrvGetGenreFlags();
	INT32 nFamily = BurnDrvGetFamilyFlags();
	
	static TCHAR szDecoratedGenre[256];
	TCHAR szFamily[256];
	
	_stprintf(szDecoratedGenre, _T(""));
	_stprintf(szFamily, _T(""));
	
 
	if (nGenre) {
		if (nGenre & GBF_HORSHOOT) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_HORSHOOT);
		}
		
		if (nGenre & GBF_VERSHOOT) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_VERSHOOT);
		}
		
		if (nGenre & GBF_SCRFIGHT) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_SCRFIGHT);
		}
		
		if (nGenre & GBF_VSFIGHT) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_VSFIGHT);
		}
		
		if (nGenre & GBF_BIOS) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_BIOS);
		}
		
		if (nGenre & GBF_BREAKOUT) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_BREAKOUT);
		}
		
		if (nGenre & GBF_CASINO) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_CASINO);
		}
		
		if (nGenre & GBF_BALLPADDLE) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_BALLPADDLE);
		}
		
		if (nGenre & GBF_MAZE) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_MAZE);
		}
		
		if (nGenre & GBF_MINIGAMES) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_MINIGAMES);
		}
		
		if (nGenre & GBF_PINBALL) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_PINBALL);
		}
		
		if (nGenre & GBF_PLATFORM) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_PLATFORM);
		}
		
		if (nGenre & GBF_PUZZLE) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_PUZZLE);
		}
		
		if (nGenre & GBF_QUIZ) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_QUIZ);
		}
		
		if (nGenre & GBF_SPORTSMISC) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_SPORTSMISC);
		}
		
		if (nGenre & GBF_SPORTSFOOTBALL) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_SPORTSFOOTBALL);
		}
		
		if (nGenre & GBF_MISC) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_MISC);
		}
		
		if (nGenre & GBF_MAHJONG) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_MAHJONG);
		}
		
		if (nGenre & GBF_RACING) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_RACING);
		}
		
		if (nGenre & GBF_SHOOT) {
			_stprintf(szDecoratedGenre, _T("%s%s, "), szDecoratedGenre, IDS_GENRE_SHOOT);
		}
		
		szDecoratedGenre[_tcslen(szDecoratedGenre) - 2] = _T('\0');
	}
	
	if (nFamily) {
		_stprintf(szFamily, _T(" ("));
		
		if (nFamily & FBF_MSLUG) {
			_stprintf(szFamily, _T("%s%s, "), szFamily, IDS_FAMILY_MSLUG);
		}
		
		if (nFamily & FBF_SF) {
			_stprintf(szFamily, _T("%s%s, "), szFamily, IDS_FAMILY_SF);
		}
		
		if (nFamily & FBF_KOF) {
			_stprintf(szFamily, _T("%s%s, "), szFamily, IDS_FAMILY_KOF);
		}
		
		if (nFamily & FBF_DSTLK) {
			_stprintf(szFamily, _T("%s%s, "), szFamily, IDS_FAMILY_DSTLK);
		}
		
		if (nFamily & FBF_FATFURY) {
			_stprintf(szFamily, _T("%s%s, "), szFamily, IDS_FAMILY_FATFURY);
		}
		
		if (nFamily & FBF_SAMSHO) {
			_stprintf(szFamily, _T("%s%s, "), szFamily, IDS_FAMILY_SAMSHO);
		}
		
		if (nFamily & FBF_19XX) {
			_stprintf(szFamily, _T("%s%s, "), szFamily, IDS_FAMILY_19XX);
		}
		
		if (nFamily & FBF_SONICWI) {
			_stprintf(szFamily, _T("%s%s, "), szFamily, IDS_FAMILY_SONICWI);
		}
		
		if (nFamily & FBF_PWRINST) {
			_stprintf(szFamily, _T("%s%s, "), szFamily, IDS_FAMILY_PWRINST);
		}		
		
		szFamily[_tcslen(szFamily) - 2] = _T(')');
		szFamily[_tcslen(szFamily) - 1] = _T('\0');
	}
 
	_stprintf(szDecoratedGenre, _T("%s%s"), szDecoratedGenre, szFamily);
	
	return szDecoratedGenre;
}

unsigned int __stdcall worker_thread_func(void *ArgList)
{

	HXUIOBJ hPreviewImage;
	HXUIOBJ hTitleImage;

	char PreviewFName[MAX_PATH+1];
	char TitleFName[MAX_PATH+1];
	char szPreviewFName[MAX_PATH+1];
	char szTitleFName[MAX_PATH+1];

	wchar_t videoPath[MAX_PATH+1];
	wchar_t previewPath[MAX_PATH+1];
	wchar_t titlePath[MAX_PATH+1];

	XSetThreadProcessor( GetCurrentThread(),2);

	strcpy(PreviewFName,szAppPreviewsPath);
	strcat(PreviewFName, BurnDrvGetTextA(DRV_NAME));
	strcat(PreviewFName, ".png");

	strcpy(TitleFName,szAppTitlesPath);
	strcat(TitleFName, BurnDrvGetTextA(DRV_NAME));
	strcat(TitleFName, ".png");

	XuiElementGetChildById( hRomListScene, L"XuiRomPreview", &hPreviewImage );
	XuiElementGetChildById( hRomListScene, L"XuiRomTitle", &hTitleImage );

	if (GetFileAttributes(TitleFName) != -1)
	{
		string titleName(TitleFName);

		titleName = ReplaceCharInString(titleName, '\\',"/");
		swprintf_s(titlePath,L"file://%S", titleName.c_str());
		XuiImageElementSetImagePath(hTitleImage, titlePath);


		swprintf_s(titlePath,L"file://%S", ReplaceCharInString(szAppTitlesPath, '\\', "/").c_str());				 
		XuiElementSetBasePath(hTitleImage, titlePath);
 
	}
	else
	{		
		XuiImageElementSetImagePath(hTitleImage, L"20866139.jpg");
	}

	if (GetFileAttributes(PreviewFName) != -1)
	{
		string titleName(PreviewFName);

		titleName = ReplaceCharInString(titleName, '\\',"/");
		swprintf_s(titlePath,L"file://%S", titleName.c_str());
		XuiImageElementSetImagePath(hPreviewImage, titlePath);


		swprintf_s(titlePath,L"file://%S", ReplaceCharInString(szAppPreviewsPath, '\\', "/").c_str());				 
		XuiElementSetBasePath(hPreviewImage, titlePath);
 
	}
	else
	{		
		XuiImageElementSetImagePath(hPreviewImage, L"20866139.jpg");
	}
 

	return 0;
}

int CRomList::PreviewTitleWorker()
{

	// create a worker thread to handle this
 	
	HANDLE hThread = (HANDLE)_beginthreadex(NULL,0,worker_thread_func,NULL,0,NULL); 
	WaitForSingleObject(hThread, INFINITE); 
	CloseHandle(hThread);	 
	return 0;
}

HRESULT CRomList::OnNotify( XUINotify *hObj, BOOL& bHandled )
{	

	int nGetTextFlags = nLoadMenuShowX & ASCIIONLY ? DRV_ASCIIONLY : 0;

	bool bUseInfo = false;
	bool bBracket = false;
	
	wchar_t videoPath[MAX_PATH+1];
	wchar_t previewPath[MAX_PATH+1];
	wchar_t titlePath[MAX_PATH+1];
	wchar_t InfoText[255] = L"";

	TCHAR szItemText[256] = _T("");

	TCHAR szUnknown[100];
	TCHAR szCartridge[100];
	TCHAR szHardware[100];

	static char szCurrentVideo[MAX_PATH+1];	
	char szRequestedVideo[MAX_PATH+1];
	char VideoFName[MAX_PATH+1];	

	HXUIOBJ hGameInfoText;
	HXUIOBJ hRomNameText;
	HXUIOBJ hRomInfoText;
	HXUIOBJ hReleasedText;
	HXUIOBJ hGenreText;
	HXUIOBJ hNotesText;

	int nIndex = 0;
	switch(hObj->dwNotify)
	{
		//case XN_SET_FOCUS:
		case XN_SELCHANGED:	
			
			nIndex = XuiListGetCurSel( this->m_hObj, NULL );

				XuiElementGetChildById( hRomListScene, L"XuiVideoPreview", &hPreviewVideo );
				XuiElementGetChildById( hRomListScene, L"XuiGameInfo", &hGameInfoText );
				XuiElementGetChildById( hRomListScene, L"XuiRomName", &hRomNameText );
				XuiElementGetChildById( hRomListScene, L"XuiROMInfo", &hRomInfoText );
				XuiElementGetChildById( hRomListScene, L"XuiReleasedBy", &hReleasedText );
				XuiElementGetChildById( hRomListScene, L"XuiGenre", &hGenreText );
				XuiElementGetChildById( hRomListScene, L"XuiNotes", &hNotesText );

			 
				if (CurrentFilter == NEOGEO_CD)
				{ 

					swprintf_s(InfoText,L"%S", ngcd_list[nIndex].szShortName );
					XuiTextElementSetText(hGameInfoText,InfoText);

					swprintf_s(InfoText,L"");
					XuiTextElementSetText(hRomNameText,InfoText);	

					swprintf_s(InfoText,L"NeoGeo CD");
					XuiTextElementSetText(hRomInfoText,InfoText);	

					swprintf_s(InfoText,L"");
					XuiTextElementSetText(hGenreText,InfoText);	

					swprintf_s(InfoText,L"%S",ngcd_list[nIndex].szPublisher);
					XuiTextElementSetText(hReleasedText,InfoText);	
 
				}
				else
				{
					nBurnDrvActive = nBurnDrv[nIndex].nBurnDrvNo; 
					swprintf_s(InfoText,L"%S", BurnDrvGetText(DRV_FULLNAME) );
					XuiTextElementSetText(hGameInfoText,InfoText);
 
					_stprintf(szItemText, _T("%s"), BurnDrvGetText(DRV_NAME));
					if ((BurnDrvGetFlags() & BDF_CLONE) && BurnDrvGetTextA(DRV_PARENT)) {

						strcpy(szRequestedVideo, szVideoPreviewPath);
						strcat(szRequestedVideo, BurnDrvGetTextA(DRV_PARENT));
						strcat(szRequestedVideo, ".wmv");

						strcpy(VideoFName, szVideoPreviewPath);
						strcat(VideoFName, BurnDrvGetTextA(DRV_PARENT));
						strcat(VideoFName, ".wmv");

						int nOldDrvSelect = nBurnDrvActive;
						TCHAR* pszName = BurnDrvGetText(DRV_PARENT);

						_stprintf(szItemText + _tcslen(szItemText), IDS_CLONE_OF, BurnDrvGetText(DRV_PARENT));

						for (nBurnDrvActive = 0; nBurnDrvActive < nBurnDrvCount; nBurnDrvActive++) {
							if (!_tcsicmp(pszName, BurnDrvGetText(DRV_NAME))) {
								break;
							}
						}
						if (nBurnDrvActive < nBurnDrvCount) {
							if (BurnDrvGetText(DRV_PARENT)) {
								_stprintf(szItemText + _tcslen(szItemText), IDS_ROMS_FROM_1, BurnDrvGetText(DRV_PARENT));
							}
						}
						nBurnDrvActive = nOldDrvSelect;
						bBracket = true;
					} else {

						strcpy(szRequestedVideo,GetVideoImage(nBurnDrv[nIndex].nBurnDrvNo,szVideoPreviewPath));				 
						strcpy(VideoFName, GetVideoImage(nBurnDrv[nIndex].nBurnDrvNo,szVideoPreviewPath));
						strcpy(szRequestedVideo,GetVideoImage(nBurnDrv[nIndex].nBurnDrvNo,szVideoPreviewPath));				 
						strcpy(VideoFName, GetVideoImage(nBurnDrv[nIndex].nBurnDrvNo,szVideoPreviewPath));

						if (BurnDrvGetTextA(DRV_PARENT)) {
							_stprintf(szItemText + _tcslen(szItemText), IDS_ROMS_FROM_2, bBracket ? _T(", ") : _T(" ("), BurnDrvGetText(DRV_PARENT));
							bBracket = true;
						}
					}
					if (BurnDrvGetTextA(DRV_SAMPLENAME)) {
						_stprintf(szItemText + _tcslen(szItemText), IDS_SAMPLES_FROM, bBracket ? _T(", ") : _T(" ("), BurnDrvGetText(DRV_SAMPLENAME));
						bBracket = true;
					}
					if (bBracket) {
						_stprintf(szItemText + _tcslen(szItemText), _T(")"));
					}
 
					swprintf_s(InfoText,L"%S", szItemText );
					XuiTextElementSetText(hRomNameText,InfoText);			 
 
					sprintf_s(szItemText,"");

					if (BurnDrvGetFlags() & BDF_PROTOTYPE) {
						_stprintf(szItemText + _tcslen(szItemText), IDS_SEL_PROTOTYPE);
						bUseInfo = true;
					}
					if (BurnDrvGetFlags() & BDF_BOOTLEG) {
						_stprintf(szItemText + _tcslen(szItemText), IDS_SEL_BOOTLEG, bUseInfo ? _T(", ") : _T(""));
						bUseInfo = true;
					}
					if (BurnDrvGetFlags() & BDF_HACK) {
						_stprintf(szItemText + _tcslen(szItemText), IDS_SEL_HACK, bUseInfo ? _T(", ") : _T(""));
						bUseInfo = true;
					}
					if (BurnDrvGetFlags() & BDF_HOMEBREW) {
						_stprintf(szItemText + _tcslen(szItemText), IDS_SEL_HOMEBREW, bUseInfo ? _T(", ") : _T(""));
						bUseInfo = true;
					}						
					if (BurnDrvGetFlags() & BDF_DEMO) {
						_stprintf(szItemText + _tcslen(szItemText), IDS_SEL_DEMO, bUseInfo ? _T(", ") : _T(""));
						bUseInfo = true;
					}
					TCHAR szPlayersMax[100];
					_stprintf(szPlayersMax, IDS_NUM_PLAYERS_MAX);
					_stprintf(szItemText + _tcslen(szItemText), IDS_NUM_PLAYERS, bUseInfo ? _T(", ") : _T(""), BurnDrvGetMaxPlayers(), (BurnDrvGetMaxPlayers() != 1) ? szPlayersMax : _T(""));
					bUseInfo = true;
					if (BurnDrvGetText(DRV_BOARDROM)) {
						_stprintf(szItemText + _tcslen(szItemText), IDS_BOARD_ROMS_FROM, bUseInfo ? _T(", ") : _T(""), BurnDrvGetText(DRV_BOARDROM));
						bUseInfo = true;
					}
			
					swprintf_s(InfoText,L"%S", szItemText );
					XuiTextElementSetText(hRomInfoText,InfoText);		

					_stprintf(szItemText, _T("%s"), GetGenreInfo());
					swprintf_s(InfoText,L"%S", szItemText );
					XuiTextElementSetText(hGenreText,InfoText);

					swprintf_s(InfoText,L"%S", BurnDrvGetTextA(DRV_COMMENT) ? BurnDrvGetText(nGetTextFlags | DRV_COMMENT) : _T(""));
					XuiTextElementSetText(hNotesText,InfoText);

					_stprintf(szUnknown, "Unknown");
					_stprintf(szCartridge, IDS_CARTRIDGE);
					_stprintf(szHardware, IDS_HARDWARE);
					_stprintf(szItemText, _T("%s (%s, %s %s)"), BurnDrvGetTextA(DRV_MANUFACTURER) ? BurnDrvGetText(nGetTextFlags | DRV_MANUFACTURER) : szUnknown, BurnDrvGetText(DRV_DATE),
					BurnDrvGetText(nGetTextFlags | DRV_SYSTEM), (BurnDrvGetHardwareCode() & HARDWARE_PREFIX_CARTRIDGE) ? szCartridge : szHardware);

					swprintf_s(InfoText,L"%S", szItemText );
					XuiTextElementSetText(hReleasedText,InfoText);
					
					PreviewTitleWorker();

					if (strcmp(szCurrentVideo,szRequestedVideo) != 0)
					{
						XuiElementSetShow(hPreviewVideo, false);
						XuiVideoPause(hPreviewVideo, true);
						strcpy(szCurrentVideo, "");
					}

					if (GetFileAttributes(VideoFName) != -1 && (strcmp(szCurrentVideo, szRequestedVideo) != 0))
					{
						strcpy(szCurrentVideo,szRequestedVideo);
						XuiElementSetShow(hPreviewVideo, true);
						PlayLoopingMovie(VideoFName); 
					}

			}
			 
			break;
	}

	return S_OK;

}

HRESULT CRomList::PlayLoopingMovie(LPCSTR szFileName)
{
  HRESULT hr;

    IXAudio2* pXAudio2 = NULL;
    IXMedia2XmvPlayer* xmvPlayer = NULL;
    IXAudio2MasteringVoice* pMasterVoice = NULL;

    // Initialize the IXAudio2 and IXAudio2MasteringVoice interfaces.
    if ( FAILED(hr = XAudio2Create( &pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR ) ) )
    {
        goto CLEANUP;
    }

    if ( FAILED(hr = pXAudio2->CreateMasteringVoice( &pMasterVoice, XAUDIO2_DEFAULT_CHANNELS,
                            XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, NULL ) ) )
    {
        goto CLEANUP;
    }


    // Create a new XMEDIA_XMV_CREATE_PARAMETERS struct.
    XMEDIA_XMV_CREATE_PARAMETERS parameters;
    ZeroMemory(&parameters, sizeof (parameters));

    // Specify that the movie should be created from a file
    // and provide the file location.
    parameters.createType = XMEDIA_CREATE_FROM_FILE;
    parameters.createFromFile.szFileName = szFileName;

    // Specify that this should be a looping movie.
    parameters.dwFlags = XMEDIA_CREATE_FOR_LOOP;
    
    // Create the player using the XMEDIA_XMV_CREATE_PARAMETERS struct.
    if( FAILED (hr = XMedia2CreateXmvPlayer(pDevice, pXAudio2, &parameters, &xmvPlayer)))
    {
        goto CLEANUP;
    }

 
#ifndef _DEBUG
	XuiVideoSetPlayer(hPreviewVideo,xmvPlayer);
	XuiVideoSetVolume(hPreviewVideo,-20.0);
#endif

    // Movie playback changes various D3D states, so you should reset the
    // states that you need after movie playback is finished.
    pDevice->SetRenderState(D3DRS_VIEWPORTENABLE, TRUE);


CLEANUP:

    // Release the audio objects
    if(pXAudio2)
    {
        pXAudio2->Release();
        pXAudio2 = 0;
    }

    // Release the movie object
    if(xmvPlayer)
    {
        xmvPlayer->Release();
        xmvPlayer = 0;
    }

    return hr;
}
 
// Add game to List
static int NeoCDList_AddGame(TCHAR* pszFile, unsigned int nGameID) 
{
	NGCDGAME* game;
	game = (NGCDGAME*)malloc(sizeof(NGCDGAME));
	memset(game, 0, sizeof(NGCDGAME));
	
	if(GetNeoGeoCDInfo(nGameID))
	{		
		memcpy(game, GetNeoGeoCDInfo(nGameID), sizeof(NGCDGAME));
		
		TCHAR szNGCDID[12];
		_stprintf(szNGCDID, _T("%04X"), nGameID);
 
		ngcd_list[nListItems].nID = nGameID;
		
		_tcscpy(ngcd_list[nListItems].szPath, pszFile);
		 
		_tcscpy(ngcd_list[nListItems].szShortName, game->pszTitle);
		_stprintf(ngcd_list[nListItems].szPublisher, _T("%s (%s)"), game->pszCompany, game->pszYear);
		
		nListItems++; 

	} else {
		// error
		return 0;
	}

	if(game) {
		free(game);
		game = NULL;
	}
	return 1;
}

static void NeoCDList_iso9660_CheckDirRecord(TCHAR* pszFile,  FILE* fp, int nSector) 
{
	int		nSectorLength		= 2048;	
	//int		nFile				= 0;
	long	lBytesRead			= 0;
	//int		nLen				= 0;
	long	lOffset				= 0;
	bool	bNewSector			= false;
	bool	bRevisionQueve		= false;
	int		nRevisionQueveID	= 0;	

	lOffset = (nSector * nSectorLength);	

	while(1) 
	{
		unsigned char nLenDR;
		memcpy(&nLenDR, iso9660_ReadOffset(fp, lOffset, 1), 1);

		if(nLenDR == 0x22) {
			lOffset		+= nLenDR;
			lBytesRead	+= nLenDR;
			continue;
		}

		if(nLenDR < 0x22) 
		{
			if(bNewSector) 
			{
				if(bRevisionQueve) {
					bRevisionQueve		= false;


					NeoCDList_AddGame(pszFile, nRevisionQueveID);
				}
				return;
			}

			nLenDR = 0;
			memcpy(&nLenDR, iso9660_ReadOffset(fp, lOffset + 1, 1), 1);

			if(nLenDR < 0x22) {
				lOffset += (2048 - lBytesRead);
				lBytesRead = 0;
				bNewSector = true;
				continue;
			}
		}

		bNewSector = false;

		unsigned char flags;
		memcpy(&flags, iso9660_ReadOffset(fp, lOffset + 25, 1), 1);

		if(!(flags & (1 << 1))) 
		{
			//MessageBoxA(NULL, "file", "", MB_OK);	

			unsigned char extentloc[8];
			memcpy(&extentloc, iso9660_ReadOffset(fp, lOffset + 2, 8), 8);

			char szValue[9];
			sprintf(szValue, "%02x%02x%02x%02x", extentloc[4], extentloc[5], extentloc[6], extentloc[7]);

			//MessageBoxA(NULL, szValue, "", MB_OK);

			unsigned int nValue = 0;
			sscanf(szValue, "%x", &nValue); 

			unsigned char data[0x10B];
			memcpy(&data, iso9660_ReadOffset(fp, (nValue * 2048), 0x10A), 0x10A);

			char szData[8];
			sprintf(szData, "%c%c%c%c%c%c%c", data[0x100], data[0x101], data[0x102], data[0x103], data[0x104], data[0x105], data[0x106]);

			if(!strncmp(szData, "NEO-GEO", 7)) 
			{
				_tcscpy(ngcd_list[nListItems].szISOFile, pszFile);				 

				char id[] = "0000";
				sprintf(id, "%02X%02X",  data[0x108], data[0x109]);

				unsigned int nID = 0;
				sscanf(id, "%x", &nID);

				unsigned char LEN_FI;
				memcpy(&LEN_FI, iso9660_ReadOffset(fp, lOffset + 32, 1), 1);

				char file[32];
				strncpy(file, (char*)iso9660_ReadOffset(fp, lOffset + 33, LEN_FI), LEN_FI);				
				file[LEN_FI] = 0;

				// King of Fighters '94, The (1994)(SNK)(JP)
				// 10-6-1994 (P1.PRG)
				if(nID == 0x0055 && data[0x67] == 0xDE) {
					// ...continue
				}

				
				// King of Fighters '94, The (1994)(SNK)(JP-US)
				// 11-21-1994 (P1.PRG)
				if(nID == 0x0055 && data[0x67] == 0xE6) {
					// Change to custom revision id
					nID = 0x1055;
				}
				
				
				// King of Fighters '95, The (1995)(SNK)(JP-US)[!][NGCD-084 MT B01, B03-B06, NGCD-084E MT B01]
				// 9-11-1995 (P1.PRG)
				if(nID == 0x0084 && data[0x6C] == 0xC0) {
					// ... continue
				}

				
				// King of Fighters '95, The (1995)(SNK)(JP-US)[!][NGCD-084 MT B10, NGCD-084E MT B03]
				// 10-5-1995 (P1.PRG)
				if(nID == 0x0084 && data[0x6C] == 0xFF) {
					// Change to custom revision id
					nID = 0x1084;
				}

				// King of Fighters '96 NEOGEO Collection, The
				if(nID == 0x0229) {
					bRevisionQueve = false;

 

					NeoCDList_AddGame(pszFile, nID);
					break;
				}

				// King of Fighters '96, The
				if(nID == 0x0214) {
					bRevisionQueve		= true;
					nRevisionQueveID	= nID;
					lOffset		+= nLenDR;
					lBytesRead	+= nLenDR;
					continue;
				}
 
				NeoCDList_AddGame(pszFile, nID);
 
				break;
			}
		}	
		
		lOffset		+= nLenDR;
		lBytesRead	+= nLenDR;
	}
	return;
}



// Check the specified ISO, and proceed accordingly
static int NeoCDList_CheckISO(TCHAR* pszFile)
{
	if(!pszFile) {
		// error
		return 0;
	}

	// Make sure we have a valid ISO file extension...
	if(_tcsstr(pszFile, _T(".iso")) || _tcsstr(pszFile, _T(".ISO")) ) 
	{
		FILE* fp = _tfopen(pszFile, _T("rb"));
		if(fp) 
		{
			// Read ISO and look for 68K ROM standard program header, ID is always there
			// Note: This function works very quick, doesn't compromise performance :)
			// it just read each sector first 264 bytes aproximately only.

			// Get ISO Size (bytes)
			fseek(fp, 0, SEEK_END);
			long lSize = 0;
			lSize = ftell(fp);
			rewind(fp);

			iso9660_PVD pvd;

			// If it has at least 16 sectors proceed
			if(lSize > (2048 * 16)) 
			{	
				// Check for Standard ISO9660 Identifier
				unsigned char IsoHeader[2048 * 16 + 1];
				unsigned char IsoCheck[6];
		
				fread(IsoHeader, 1, 2048 * 16 + 1, fp); // advance to sector 16 and PVD Field 2
				fread(IsoCheck, 1, 5, fp);	// get Standard Identifier Field from PVD
		
				// Verify that we have indeed a valid ISO9660 MODE1/2048
				if(!memcmp(IsoCheck, "CD001", 5))
				{
					//bprintf(PRINT_NORMAL, _T("    Standard ISO9660 Identifier Found. \n"));
					iso9660_VDH vdh;

					// Get Volume Descriptor Header			
					memset(&vdh, 0, sizeof(vdh));
					memcpy(&vdh, iso9660_ReadOffset(fp, (2048 * 16), sizeof(vdh)), sizeof(vdh));

					// Check for a valid Volume Descriptor Type
					if(vdh.vdtype == 0x01) 
					{
						// Get Primary Volume Descriptor
						memset(&pvd, 0, sizeof(pvd));
						memcpy(&pvd, iso9660_ReadOffset(fp, (2048 * 16), sizeof(pvd)), sizeof(pvd));

						// ROOT DIRECTORY RECORD

						// Handle Path Table Location
						char szRootSector[32];
						unsigned int nRootSector = 0;

						sprintf(szRootSector, "%02X%02X%02X%02X", 
							pvd.root_directory_record.location_of_extent[4], 
							pvd.root_directory_record.location_of_extent[5], 
							pvd.root_directory_record.location_of_extent[6], 
							pvd.root_directory_record.location_of_extent[7]);

						// Convert HEX string to Decimal
						sscanf(szRootSector, "%X", &nRootSector);

						// Process the Root Directory Records
						NeoCDList_iso9660_CheckDirRecord(pszFile, fp, nRootSector);

						// Path Table Records are not processed, since NeoGeo CD should not have subdirectories
						// ...
					}
				} else {

					//bprintf(PRINT_NORMAL, _T("    Standard ISO9660 Identifier Not Found, cannot continue. \n"));

					_tcscpy(ngcd_list[nListItems].szPathCUE,  "");	
					ngcd_list[nListItems].bFoundCUE = false;
					return 0;
				}
			}
		} else {

			//bprintf(PRINT_NORMAL, _T("    Couldn't open %s \n"), GetIsoPath());
			return 0;
		}

		if(fp) fclose(fp);

	} else {

		//bprintf(PRINT_NORMAL, _T("    File doesn't have a valid ISO extension [ .iso / .ISO ] \n"));
		return 0;
	}

	return 1;
}

// This will parse the specified CUE file and return the ISO path, if found
static TCHAR* NeoCDList_ParseCUE(TCHAR* pszFile) 
{
	//if(!pszFile) return NULL;

	TCHAR* szISO = NULL;
	szISO = (TCHAR*)malloc(sizeof(TCHAR) * 2048);
	if(!szISO) return NULL;

	// open file
	FILE* fp = NULL;
	fp = _tfopen(pszFile, _T("r"));

	ngcd_list[nListItems].bFoundCUE = false;

	if(!fp) {
		return NULL;
	}

	

	while(!feof(fp))
	{
		TCHAR szBuffer[2048];
		TCHAR szOriginal[2048];

		_fgetts(szBuffer, sizeof(szBuffer), fp);

		// terminate string
		szBuffer[260] = 0;

		if(!*szBuffer) {
			return NULL;
		}

		int nLength = 0;
		nLength = _tcslen(szBuffer);

		// Remove ASCII control characters from the string (including the 'space' character)
		while (szBuffer[nLength-1] < 32 && nLength > 0)
		{
			szBuffer[nLength-1] = 0;
			nLength--;
		}

		_tcscpy(szOriginal, szBuffer);

		if(!_tcsncmp(szBuffer, _T("FILE"), 4))
		{
			TCHAR* pEnd = _tcsrchr(szBuffer, '"');
			if (!pEnd)	{
				break;	// Invalid CUE format
			}

			*pEnd = 0;

			TCHAR* pStart = _tcschr(szBuffer, '"');

			if(!pStart)	{
				break;	// Invalid CUE format
			}

			if(!_tcsncmp(pEnd + 2, _T("BINARY"), 6))
			{
				_tcscpy(szISO,  pStart + 1);
				ngcd_list[nListItems].bFoundCUE = true;
				_tcscpy(ngcd_list[nListItems].szPathCUE,  pszFile);
				_tcscpy(ngcd_list[nListItems].szISOFile,  pStart + 1);
			} 
			
			if (!_tcsncmp(pEnd + 2, _T("WAVE"), 5)) {
				if(!ngcd_list[nListItems].nAudioTracks) {
					ngcd_list[nListItems].nAudioTracks = 0;
				}

				_tcscpy(ngcd_list[nListItems].szTracks[ngcd_list[nListItems].nAudioTracks], pStart + 1);

				ngcd_list[nListItems].nAudioTracks++;
			} 
		}
	}
	if(fp) fclose(fp);

	return szISO;
}

 
HRESULT CRomList::OnRescanRoms( char *szPath,  BOOL& bHandled )
{ 
	HXUIOBJ hRomCountText;
	
	InitRomList();

	DeleteItems(0, nTmpDrvCount);

	// check if we are scanning neocdz or regular roms

	if (CurrentFilter == NEOGEO_CD)
	{ 
		WIN32_FIND_DATA ffdDirectory;

		HANDLE hDirectory = NULL;
		memset(&ffdDirectory, 0, sizeof(WIN32_FIND_DATA));
		
		// Scan main dir for sub-directories
		TCHAR szSearch[2048] = _T("\0");

		_stprintf(szSearch, _T("%s*"), szNeoCDGamesDir);
	
		hDirectory = FindFirstFile(szSearch, &ffdDirectory);

		if (hDirectory == INVALID_HANDLE_VALUE) {
			// error
		} else {
			do 
			{
			// DIRECTORY
			if((ffdDirectory.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				if(!_tcscmp(ffdDirectory.cFileName, _T(".")) || !_tcscmp(ffdDirectory.cFileName, _T("..")))
				{
					// lets ignore " . " and " .. "
					continue;
				}

				TCHAR* pszISO = NULL;
				pszISO = (TCHAR*)malloc(sizeof(TCHAR) * 512);
				
				bool bDone = false;

				WIN32_FIND_DATA ffdSubDirectory;

				HANDLE hSubDirectory = NULL;
				memset(&ffdSubDirectory, 0, sizeof(WIN32_FIND_DATA));

				TCHAR szSubSearch[512] = _T("\0");

 
					// Scan sub-directory for CUE									
					_stprintf(szSubSearch, _T("%s%s\\*.cue"), szNeoCDGamesDir, ffdDirectory.cFileName);

					hSubDirectory = FindFirstFile(szSubSearch, &ffdSubDirectory);
				
					if (hSubDirectory == INVALID_HANDLE_VALUE) {
						// error
					} else {
		
						do
						{
							// Sub-directories only
							if(!(ffdSubDirectory.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
							{
								// File is CUE
								if(_tcsstr(ffdSubDirectory.cFileName, _T(".cue")) || _tcsstr(ffdSubDirectory.cFileName, _T(".CUE")))
								{
									// Parse CUE
									TCHAR szParse[512] = _T("\0");				
									_stprintf(szParse, _T("%s%s\\%s"), szNeoCDGamesDir, ffdDirectory.cFileName, ffdSubDirectory.cFileName);

									//MessageBox(NULL, szParse, _T(""), MB_OK);

									pszISO = NeoCDList_ParseCUE( szParse );

									TCHAR szISO[512] =_T("\0");
									_stprintf(szISO, _T("%s%s\\%s"), szNeoCDGamesDir, ffdDirectory.cFileName,  pszISO);

									NeoCDList_CheckISO(szISO);
									bDone = true;

									break; // no need to continue				
								}
							}
						} while(FindNextFile(hSubDirectory, &ffdSubDirectory));
					}

					if(pszISO) {
						pszISO[0] = 0;
					}

					if(bDone) {
						FindClose(hSubDirectory);
						continue;
					}
 

				hSubDirectory = NULL;
				memset(&ffdSubDirectory, 0, sizeof(WIN32_FIND_DATA));

				// Scan sub-directory for ISO
				_stprintf(szSubSearch, _T("%s%s\\*.iso"), szNeoCDGamesDir, ffdDirectory.cFileName);

				hSubDirectory = FindFirstFile(szSubSearch, &ffdSubDirectory);

				if (hSubDirectory == INVALID_HANDLE_VALUE) {
					// error
				} else {
		
					do
					{
						// Sub-directories only
						if(!(ffdSubDirectory.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
						{
							// File is ISO
							if(_tcsstr(ffdSubDirectory.cFileName, _T(".iso")) || _tcsstr(ffdSubDirectory.cFileName, _T(".ISO")))
							{
								TCHAR szISO[512] = _T("\0");				
								_stprintf(szISO, _T("%s%s\\%s"), szNeoCDGamesDir, ffdDirectory.cFileName, ffdSubDirectory.cFileName);

								NeoCDList_CheckISO(szISO);

								break; // no need to continue
							}
						}
					} while(FindNextFile(hSubDirectory, &ffdSubDirectory));
				}
			} else {
				// FILE
			}
		} while(FindNextFile(hDirectory, &ffdDirectory));			
	}
	FindClose(hDirectory);
	AvRoms();	 
	InsertItems( 0, nListItems );
 
	}
	else
	{
		 
		// regular roms...

		int tempgame;
		tempgame = nBurnDrvSelect[0];
 
		InitRomList();
		SelListMake();

		AvRoms();
		nBurnDrvSelect[0] = tempgame;
		InsertItems( 0, nTmpDrvCount );
 
		XuiElementGetChildById( hRomListScene, L"XuiNumberOfRoms", &hRomCountText );
		XuiTextElementSetText(hRomCountText,szRomsAvailableInfo);
	}

	bHandled = TRUE;	
    return( S_OK );
}

HRESULT CRomList::OnInit(XUIMessageInit *pInitData, BOOL& bHandled)
{
 
	XUIMessage xuiMsg;
	InRescanRomsStruct msgData;
	InRescanRomsFirstFunc( &xuiMsg, &msgData,NULL );
	XuiSendMessage( m_hObj, &xuiMsg );

	bHandled = TRUE;
    return S_OK;
}

HRESULT CRomList::OnGetItemCountAll(
        XUIMessageGetItemCount *pGetItemCountData, 
        BOOL& bHandled)
    {
		if (CurrentFilter == NEOGEO_CD)
			pGetItemCountData->cItems = nListItems;
		else
			pGetItemCountData->cItems = nTmpDrvCount;

        bHandled = TRUE;
        return S_OK;
    }


// Gets called every frame
HRESULT CRomList::OnGetSourceDataText(
    XUIMessageGetSourceText *pGetSourceTextData, 
    BOOL& bHandled)
{
	
    
    if( ( 0 == pGetSourceTextData->iData ) && ( ( pGetSourceTextData->bItemData ) ) ) {

			LPCWSTR lpszwBuffer = NULL;

			if (CurrentFilter == NEOGEO_CD)
			{ 
				lpszwBuffer = MultiCharToUniChar((char *)ngcd_list[pGetSourceTextData->iItem].szShortName);
			}
			else
			{
				lpszwBuffer = MultiCharToUniChar((char *)nBurnDrv[pGetSourceTextData->iItem].pszDisplayName);
			}


            pGetSourceTextData->szText = lpszwBuffer;

            bHandled = TRUE;
        }
        return S_OK;

}


std::string ReplaceCharInString(  
    const std::string & source, 
    char charToReplace, 
    const std::string replaceString 
    ) 
{ 
    std::string result; 
 
    // For each character in source string: 
    const char * pch = source.c_str(); 
    while ( *pch != '\0' ) 
    { 
        // Found character to be replaced? 
        if ( *pch == charToReplace ) 
        { 
            result += replaceString; 
        } 
        else 
        { 
            // Just copy original character 
            result += (*pch); 
        } 
 
        // Move to next character 
        ++pch; 
    } 
 
    return result; 
} 
 