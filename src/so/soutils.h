#include <string>
#include <SDL.h>

#ifdef _XBOX
#include <xtl.h>
#endif

typedef void (*SOUtilsKeyboardCallback)(const std::string& text, void* userData);

class SOUtils{
public:
	SOUtils();
	~SOUtils();

	static void pedirTextoAsync(std::string titulo, std::string subtitulo, std::string textoPorDefecto,
							  SOUtilsKeyboardCallback callback, void* userData);
	static void updateKeyboard();

private:
#ifdef _XBOX
	static XOVERLAPPED s_overlapped;
	static WCHAR s_buffer[512];
	static SOUtilsKeyboardCallback s_callback;
	static void* s_userData;
	static bool s_pending;
	static wchar_t* wTitulo;
    static wchar_t* wSub;
	static wchar_t* wDefaultText;
#endif
};