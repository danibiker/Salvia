#include "so/soutils.h"

#ifdef _XBOX

XOVERLAPPED SOUtils::s_overlapped = {0};
WCHAR SOUtils::s_buffer[512] = {0};
SOUtilsKeyboardCallback SOUtils::s_callback = NULL;
void* SOUtils::s_userData = NULL;
bool SOUtils::s_pending = false;

void SOUtils::pedirTextoAsync(const std::string& titulo, const std::string& subtitulo,
							  SOUtilsKeyboardCallback callback, void* userData)
{
	int lenTitulo = MultiByteToWideChar(CP_ACP, 0, titulo.c_str(), -1, NULL, 0);
	int lenSub = MultiByteToWideChar(CP_ACP, 0, subtitulo.c_str(), -1, NULL, 0);

	wchar_t* wTitulo = new wchar_t[lenTitulo];
	wchar_t* wSub = new wchar_t[lenSub];

	MultiByteToWideChar(CP_ACP, 0, titulo.c_str(), -1, wTitulo, lenTitulo);
	MultiByteToWideChar(CP_ACP, 0, subtitulo.c_str(), -1, wSub, lenSub);

	s_pending = true;
	s_callback = callback;
	s_userData = userData;

	XShowKeyboardUI(0, VKBD_DEFAULT, L"", wTitulo, wSub, s_buffer, 512, &s_overlapped);

	delete[] wTitulo;
	delete[] wSub;
}

void SOUtils::updateKeyboard()
{
	if (!s_pending) return;
	if (!XHasOverlappedIoCompleted(&s_overlapped)) return;

	s_pending = false;

	std::string texto;
	if (XGetOverlappedExtendedError(&s_overlapped) == ERROR_SUCCESS)
	{
		int tam = WideCharToMultiByte(CP_ACP, 0, s_buffer, -1, NULL, 0, NULL, NULL);
		texto.resize(tam - 1);
		WideCharToMultiByte(CP_ACP, 0, s_buffer, -1, &texto[0], tam, NULL, NULL);
	}

	if (s_callback) s_callback(texto, s_userData);
}

#else

void SOUtils::pedirTextoAsync(const std::string& titulo, const std::string& subtitulo,
							  SOUtilsKeyboardCallback callback, void* userData)
{
	// No implementado en esta plataforma
}

void SOUtils::updateKeyboard()
{
	// No implementado en esta plataforma
}

#endif
