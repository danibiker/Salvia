// State dialog module
#include "burner.h"

int bDrvSaveAll = 0;
 
// The automatic save
int StatedAuto(int bSave)
{
	static TCHAR szName[32] = _T("");
	int nRet;

	_stprintf(szName, _T("GAME:\\config\\games\\%s.fs"), BurnDrvGetText(DRV_NAME));

	if (bSave == 0) {
		nRet = BurnStateLoad(szName, bDrvSaveAll, NULL);		// Load ram
		if (nRet && bDrvSaveAll)	{
			nRet = BurnStateLoad(szName, 0, NULL);				// Couldn't get all - okay just try the nvram
		}
	} else {
		nRet = BurnStateSave(szName, bDrvSaveAll);				// Save ram
	}

	return nRet;
}

static void CreateStateName(int nSlot)
{
	_stprintf(szChoice, _T("GAME:\\savestates\\%s slot %02x.fs"), BurnDrvGetText(DRV_NAME), nSlot);
}

int StatedLoad(int nSlot)
{
	TCHAR szFilter[1024];
	int nRet;
	int bOldPause;

	if (nSlot) {
		CreateStateName(nSlot);
	} else {
		if (bDrvOkay) {
			_stprintf(szChoice, _T("%s*.fs"), BurnDrvGetText(DRV_NAME));
		} else {
			_stprintf(szChoice, _T("savestate"));
		}
		 
		bOldPause = bRunPause;
		bRunPause = 1;
		nRet = 1;
		bRunPause = bOldPause;
  
	}

	nRet = BurnStateLoad(szChoice, 1, &DrvInitCallback);

	if (nSlot) {
		return nRet;
	}

	// Describe any possible errors:
	if (nRet == 3) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_DISK_THIS_STATE));
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DISK_UNAVAIL));
	} else {
		if (nRet == 4) {
			FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_DISK_THIS_STATE));
			FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DISK_TOOOLD), _T(APP_TITLE));
		} else {
			if (nRet == 5) {
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_DISK_THIS_STATE));
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DISK_TOONEW), _T(APP_TITLE));
			} else {
				if (nRet && !nSlot) {
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DISK_LOAD));
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_DISK_STATE));
				}
			}
		}
	}

	if (nRet) {
		FBAPopupDisplay(PUF_TYPE_ERROR);
	}

	return nRet;
}

int StatedSave(int nSlot)
{
	TCHAR szFilter[1024];
	int nRet;
	int bOldPause;

	if (bDrvOkay == 0) {
		return 1;
	}
	
	if (nSlot) {
		CreateStateName(nSlot);
	} else {
		_stprintf(szChoice, _T("%s"), BurnDrvGetText(DRV_NAME));
		 
		bOldPause = bRunPause;
		bRunPause = 1;
		nRet = 1;
		bRunPause = bOldPause;

		if (nRet == 0) {		// Error
			return 1;
		}
	}

	nRet = BurnStateSave(szChoice, 1);

	if (nRet && !nSlot) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DISK_CREATE));
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_DISK_STATE));
		FBAPopupDisplay(PUF_TYPE_ERROR);
	}

	return nRet;
}
