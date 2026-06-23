//--------------------------------------------------------------------------------------
// XuiTutorial.cpp
//
// Shows how to display and use a simple XUI scene.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#include <algorithm>
#include <new>
#include <iostream>
#include <vector>

#ifndef ROMLIST_H
#define ROMLIST_H
 
#define XM_MESSAGE_ON_RESCAN_ROMS  XM_USER

typedef struct
{
    char *szPath;    
}
InRescanRomsStruct;


int BurnerLoadDriver(TCHAR *szDriverName);
void InRescanRomsFirstFunc(XUIMessage *pMsg, InRescanRomsStruct* pData, char *szPath);

// Define the message map macro
#define XUI_ON_XM_MESSAGE_ON_RESCAN_ROMS(MemberFunc)\
    if (pMessage->dwMessage == XM_MESSAGE_ON_RESCAN_ROMS)\
    {\
        InRescanRomsStruct *pData = (InRescanRomsStruct *) pMessage->pvData;\
        return MemberFunc(pData->szPath,  pMessage->bHandled);\
    }

class CRomList : CXuiListImpl
{
public:

	XUI_IMPLEMENT_CLASS(CRomList, L"BurnRomList", XUI_CLASS_LIST);

	XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT(OnInit)
		XUI_ON_XM_GET_SOURCE_TEXT(OnGetSourceDataText)
		XUI_ON_XM_GET_ITEMCOUNT_ALL(OnGetItemCountAll)
		//XUI_ON_XM_GET_CURSEL(OnGetCurSel)
		XUI_ON_XM_NOTIFY( OnNotify )
		XUI_ON_XM_MESSAGE_ON_RESCAN_ROMS( OnRescanRoms )
	XUI_END_MSG_MAP()

	CRomList();

	int FreeRomList();
	int AvRoms();
	int RefreshRomList();
	int InitRomList();

	int RomCount;
	bool RomListOK;	

	int PreviewTitleWorker();
 
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
	HRESULT OnNotify( XUINotify *hObj, BOOL& bHandled );
	HRESULT OnGetSourceDataText(XUIMessageGetSourceText *pGetSourceTextData, BOOL& bHandled);
	HRESULT OnGetItemCountAll(XUIMessageGetItemCount *pGetItemCountData, BOOL& bHandled);
	HRESULT OnRescanRoms ( char *szPath, BOOL& bHandled );

	HRESULT PlayLoopingMovie(LPCSTR szFileName);
	 
};
 
//--------------------------------------------------------------------------------------
// Scene implementation class.
//--------------------------------------------------------------------------------------
class CRomListScene : public CXuiSceneImpl
{

protected:

    // Control and Element wrapper objects.
    CXuiControl m_AddToFavorites;
    CXuiControl m_SkinImage;
	CXuiControl m_Back; 
	CXuiElement m_PreviewImage;
	CXuiElement m_TitleImage;
	CXuiList m_RomList;
	CXuiVideo   m_BackVideo;
	CXuiTextElement m_DeviceText;
	CXuiTextElement m_ConsoleOutput;
	CXuiTextElement m_FreeMemory;
	CXuiControl m_NextDevice;
	CXuiControl m_PrevDevice;
	CXuiControl m_NextDevice1;
	CXuiControl m_PrevDevice1;
	CXuiControl m_MVSMultiSlot;
	CXuiControl m_LaunchRom;

	CXuiElement m_MVSMultiSlotScene;
	CXuiControl m_ClearMultiSlot;
	CXuiControl m_StartMultiSlot;
	CXuiControl m_RescanRoms;

	CXuiTextElement m_GameInfo;
	CXuiTextElement m_RomName;
	CXuiTextElement m_RomInfo;
	CXuiTextElement m_ReleasedBy;
	CXuiTextElement m_Genre;
	CXuiTextElement m_Notes;

	CXuiTextElement m_Version;
	CXuiTextElement m_Hardware;
	CXuiTextElement m_NumberOfGames;

	CXuiCheckbox m_HideChildren;
	CXuiCheckbox m_34PlayerOnly;
	CXuiCheckbox m_UseArcadeStickSettings;
	CXuiCheckbox m_HighScore;

 
   

    // Message map.
    XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT( OnInit )
		XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
		//XUI_ON_XM_RENDER( OnRender )
    XUI_END_MSG_MAP()

	


 
public:
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
	HRESULT OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled );
	//HRESULT OnRender(  XUIMessageRender *pRenderData, BOOL& bHandled );
	void DoMVSSlotMode(int index);
 
public:

    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
    XUI_IMPLEMENT_CLASS( CRomListScene, L"RomListScene", XUI_CLASS_SCENE )
};


#endif