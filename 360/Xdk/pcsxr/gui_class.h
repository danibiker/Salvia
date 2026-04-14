#include <xtl.h>
#include <xboxmath.h>
#include <xui.h>
#include <xuiapp.h>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include "misc.h"

class FileBrowserProvider {
protected:
	struct _FILE_INFO {
		std::wstring filename;
		std::wstring displayname;
		std::wstring gamecover;
		bool isDir;
		
	};

	std::wstring currentDir;
	std::vector<_FILE_INFO> fileList;

	 
protected:

	static bool compare (const _FILE_INFO a, const _FILE_INFO b) {
		
		/* If one is a file and one is a directory the directory is first. */
		if(a.isDir && !b.isDir) return true;
        if(!a.isDir && b.isDir) return false;

		return a.displayname < b.displayname;
	}

	void Sort() {
		std::sort(fileList.begin(), fileList.end(), compare);
	}
	
public:
	void Init() {
		currentDir = L"game:";
		
	}

	bool endsWith (std::string const &fullString, std::string const &ending)
	{
		if (fullString.length() >= ending.length()) {
			return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
		} else {
			return false;
		}
	}

	bool startWith(std::string const &fullstring, char const &filter)
	{
		if (fullstring[0] == filter)
			return true;
		else
			return false;
	}



	void AddParentEntry(std::wstring currentDir) {
		_FILE_INFO finfo;
		wchar_t lastLetter = currentDir[currentDir.length() - 1];
		if (lastLetter == L':') {
			finfo.isDir = true;
			finfo.displayname = L"DeviceList";
			finfo.filename = L"/";
			finfo.gamecover = L"file://game:/media/PsxSkin.xzp#media\\Graphics\\hdd.png";

			fileList.push_back(finfo);
			return;
		}

		unsigned int p = currentDir.rfind(L"\\");
		currentDir = currentDir.substr(0,  p);

		finfo.isDir = true;
		finfo.displayname = L"..";
		finfo.gamecover = L"file://game:/media/PsxSkin.xzp#media\\Graphics\\back.png";
		
		finfo.filename = currentDir;

		fileList.push_back(finfo);
	}

	HRESULT AddDevicesList() {
		// Free file list
		fileList.clear();

		_FILE_INFO finfo;
		finfo.isDir = true;
		
		finfo.filename = L"game:";
		finfo.displayname = L"[GAME]";
        finfo.gamecover = L"file://game:/media/PsxSkin.xzp#media\\Graphics\\psx.jpg";
		fileList.push_back(finfo);

		finfo.filename = L"usb0:";
		finfo.displayname = L"[USB0]";
		finfo.gamecover = L"file://game:/media/PsxSkin.xzp#media\\Graphics\\usb.png";
		fileList.push_back(finfo);

		finfo.filename = L"usb1:";
		finfo.displayname = L"[USB1]";
		finfo.gamecover = L"file://game:/media/PsxSkin.xzp#media\\Graphics\\usb.png";
		fileList.push_back(finfo);

		finfo.filename = L"hdd0:";
		finfo.displayname = L"[HDD0]";
		finfo.gamecover = L"file://game:/media/PsxSkin.xzp#media\\Graphics\\hdd.png";
		fileList.push_back(finfo);

		finfo.filename = L"hdd1:";
		finfo.displayname = L"[HDD1]";
		finfo.gamecover = L"file://game:/media/PsxSkin.xzp#media\\Graphics\\hdd.png";
		fileList.push_back(finfo);
		
		finfo.filename = L"hdd2:";
		finfo.displayname = L"[HDD2]";
		finfo.gamecover = L"file://game:/media/PsxSkin.xzp#media\\Graphics\\hdd.png";
		fileList.push_back(finfo);
		
		finfo.filename = L"hdd3:";
		finfo.displayname = L"[HDD3]";
		finfo.gamecover = L"file://game:/media/PsxSkin.xzp#media\\Graphics\\hdd.png";
		fileList.push_back(finfo);

		return S_OK;
	}

	HRESULT UpdateDirList(std::wstring currentDir = L"game:\\ROMS") {
		WIN32_FIND_DATA ffd;
		std::string szDir;

		HANDLE hFind = INVALID_HANDLE_VALUE;
		DWORD dwError=0;		

		if(currentDir == L"/") {
			AddDevicesList();
			return S_OK;
		}
		// Free file list
		fileList.clear();
		AddParentEntry(currentDir);

		get_string(currentDir, szDir);
		szDir = szDir + "\\*";

		// Look for files in current directory
		hFind = FindFirstFile(szDir.c_str(), &ffd);
		
		if (INVALID_HANDLE_VALUE == hFind) 
		{
			return S_FALSE;
		} 

		// List all the files in the directory with some info about them.
		do
		{
			_FILE_INFO finfo;
			finfo.isDir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)?true:false;
			get_wstring(ffd.cFileName, finfo.displayname);
			finfo.filename = currentDir + L"\\" + finfo.displayname;
			

			
			// :s

			//teste adding cover into scrool items
			if (finfo.isDir) {
				finfo.displayname = L"["+finfo.displayname+L"]";
			    finfo.gamecover = L"file://game:/media/PsxSkin.xzp#media\\Graphics\\folder.png";
			}
			else{
				std::string coverpath = "game:\\covers\\" + std::string(finfo.displayname.begin(),finfo.displayname.end());
				
				coverpath.append(".jpg");
				
                if (fileExists((char *)coverpath.c_str()))
				finfo.gamecover = L"file://game:\\covers\\" + finfo.displayname + L".jpg";
				else
				finfo.gamecover = L"file://game:/media/PsxSkin.xzp#media\\psx.jpg";

			}


			if(!xboxConfig.noGameBrowse){

			//ext <- rom extension check
			std::string ext(finfo.displayname.begin(),finfo.displayname.end());
			if (endsWith(ext,".img"))
				fileList.push_back(finfo);
			else
				if (endsWith(ext,".bin"))
					fileList.push_back(finfo);			
				else
					if (endsWith(ext,".mdf"))
						fileList.push_back(finfo);
					else
						if (endsWith(ext,".iso"))
							fileList.push_back(finfo);
						else
							if (endsWith(ext,".IMG"))
								fileList.push_back(finfo);
							else
								if (endsWith(ext,".BIN"))
									fileList.push_back(finfo);			
								else
									if (endsWith(ext,".MDF"))
										fileList.push_back(finfo);
									else
										if (endsWith(ext,".ISO"))
											fileList.push_back(finfo);
										else
											if (finfo.isDir)
												fileList.push_back(finfo);
			}else
				fileList.push_back(finfo);

		}
		while (FindNextFile(hFind, &ffd) != 0);

		FindClose(hFind);

		Sort();

		return S_OK;
	}



// ÛpdateDirList filter mode
	HRESULT UpdateDirListFilterMode(std::wstring currentDir = L"game:\\ROMS", char letter =(char)"") {
		WIN32_FIND_DATA ffd;
		std::string szDir;

		HANDLE hFind = INVALID_HANDLE_VALUE;
		DWORD dwError=0;		

		if(currentDir == L"/") {
			AddDevicesList();
			return S_OK;
		}
		// Free file list
		fileList.clear();
		AddParentEntry(currentDir);

		get_string(currentDir, szDir);
		szDir = szDir + "\\*";

		// Look for files in current directory
		hFind = FindFirstFile(szDir.c_str(), &ffd);
		
		if (INVALID_HANDLE_VALUE == hFind) 
		{
			return S_FALSE;
		} 

		// List all the files in the directory with some info about them.
		do
		{
			_FILE_INFO finfo;
			finfo.isDir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)?true:false;
			get_wstring(ffd.cFileName, finfo.displayname);
			finfo.filename = currentDir + L"\\" + finfo.displayname;
			

			
			// :s

			//teste adding cover into scrool items
			if (finfo.isDir) {
				finfo.displayname = L"["+finfo.displayname+L"]";
			    finfo.gamecover = L"file://game:/media/PsxSkin.xzp#media\\Graphics\\folder.png";
			}
			else{
				std::string coverpath = "game:\\covers\\" + std::string(finfo.displayname.begin(),finfo.displayname.end());
				
				coverpath.append(".jpg");
				
                if (fileExists((char *)coverpath.c_str()))
				finfo.gamecover = L"file://game:\\covers\\" + finfo.displayname + L".jpg";
				else
				finfo.gamecover = L"file://game:/media/PsxSkin.xzp#media\\psx.jpg";

			}


			if(!xboxConfig.noGameBrowse){

			//ext <- rom extension check
			std::string ext(finfo.displayname.begin(),finfo.displayname.end());
			if (startWith(ext,letter)){

				if (endsWith(ext,".img"))
					fileList.push_back(finfo);
				else
					if (endsWith(ext,".bin"))
						fileList.push_back(finfo);			
					else
						if (endsWith(ext,".mdf"))
							fileList.push_back(finfo);
						else
							if (endsWith(ext,".iso"))
								fileList.push_back(finfo);
							else
								if (endsWith(ext,".IMG"))
									fileList.push_back(finfo);
								else
									if (endsWith(ext,".BIN"))
										fileList.push_back(finfo);			
									else
										if (endsWith(ext,".MDF"))
											fileList.push_back(finfo);
										else
											if (endsWith(ext,".ISO"))
												fileList.push_back(finfo);
											else
												if (finfo.isDir)
													fileList.push_back(finfo);




				}else
					fileList.push_back(finfo);
			}// apenas adiciona itens com letra

		}
		while (FindNextFile(hFind, &ffd) != 0);

		FindClose(hFind);

		Sort();

		return S_OK;
	}
	DWORD Size() {
		return this->fileList.size();
	}

	LPCWSTR At(unsigned int i) {
		if (i >= 0 && i < Size()) {
			return this->fileList[i].displayname.c_str();
		} else {
			return L"";
		}
	}

	LPCWSTR GameCover(unsigned int i) {
	   if (i >= 0 && i < Size()) {
		   return this->fileList[i].gamecover.c_str();
		} else {
			return L"";
		}
	}

	LPCWSTR Filename(unsigned int i) {
		if (i >= 0 && i < Size()) {
			return this->fileList[i].filename.c_str();
		} else {
			return L"";
		}
	}

	bool IsDir(unsigned int i) {
		if (i >= 0 && i < Size()) {
			return this->fileList[i].isDir;
		} else {
			return false;
		}
	}
};