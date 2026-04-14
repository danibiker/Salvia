#include <xtl.h>
#include <xboxmath.h>
#include <xui.h>
#include <xuiapp.h>
#include <string>
#include <vector>
#include <stdint.h>


// tombraiderfix
extern "C" BOOL tombraider2fix;
//frontmissionfix
extern "C" BOOL frontmission3fix;
//gpu
extern "C" BOOL gpuThreadrunning;

// D3d
extern void InitD3D();
extern void VideoPresent();
extern IDirect3DDevice9*				g_pd3dDevice;
extern D3DPRESENT_PARAMETERS			g_d3dpp;

// Pcsx Helper
extern void ResetPcsx();
extern void ShutdownPcsx();
extern void RunPcsx(char* game);
extern void PausePcsx();
extern void ResumePcsx();
extern void DrawPcsxSurface();

extern void LoadSettings();
extern void ApplySettings(const char* path);


extern "C" void RemoveSound();
extern "C" boolean use_vm;

extern void SaveStatePcsx(int n);
extern void LoadStatePcsx(int n);
extern void LoadStatePcsx(std::string save);
extern void ChangeDisc(std::string game);
//extern void DoPcsx(char*game);
extern void DoPcsx(std::string game);
extern void CoverSystemAndGameProfile(std::string gameID);

extern  void LoadShaderFromFile(const char* filename);

//extern std::wstring GamePath;

// Peops and video 
extern bool     linearfilter;
extern "C" int  UseFrameLimit;
extern "C" int  darkforcesfix;
extern "C" BOOL spuirq;


extern "C" void gpuDmaThreadInit();
extern "C" void POKOPOM_Init();


std::string get_string(const std::wstring & s, std::string & d);
std::wstring get_wstring(const std::string & s, std::wstring & d);
std::wstring string2wstring(const std::string& s);
	


HRESULT ShowKeyboard(std::wstring & resultText, std::wstring titleText = L"", std::wstring descriptionText = L"", std::wstring defaultText = L"");

// Xui Main app
class CMyApp : public CXuiModule
{
protected:
    virtual HRESULT RegisterXuiClasses();
    virtual HRESULT UnregisterXuiClasses();
};

// Pcsx xbox config
class XboxConfig {
public:
	std::string game;
	bool Running;
	bool ShutdownRequested;
	bool ResetRequested;
	bool OsdMenuRequested;
	bool UseDynarec;
	bool UseSpuIrq;
	bool UseThreadedGpu;
	bool UseFrameLimiter;
	bool UseParasiteEveFix;
	int  UseDarkForcesFix; 
	bool runBios;
	bool noGameBrowse;
	char Shaders[4096];
    char IsoFolder[4096];
	char Game2Boot[4096];
	char CurrentgameID[4096];
	char CurrentgameIDFullPath[4096];
	char GameIDCover[4096];
	bool CoverModeHorizontal;
	int  region;
	bool UseSlowBoot;
	bool UseLinearFilter;
	bool UseCpuBias;
	bool UseTombRaider2Fix;
	bool UseFrontMission3Fix;
	bool AutobootIso;
	bool AutobootIsoFromAurora;
	std::string saveStateDir;

	int InfoBoxType; //1 - Information ;  2 - Error
	bool InfoboxShow;
	LPCWSTR Infomsg;


	std::wstring CoverPath;
	std::wstring GamePath;
	std::wstring GameName;
};


extern XboxConfig xboxConfig;
extern CMyApp app;

extern void LoadShader(std::string game);
extern void ApplyShader(std::string game);
extern void LoadAutobootIsoConfig();






