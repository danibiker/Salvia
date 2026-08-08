#include "so/soutils.h"

#ifdef _XBOX

XOVERLAPPED SOUtils::s_overlapped = {0};
WCHAR SOUtils::s_buffer[512] = {0};
SOUtilsKeyboardCallback SOUtils::s_callback = NULL;
void* SOUtils::s_userData = NULL;
wchar_t* SOUtils::wTitulo = NULL;
wchar_t* SOUtils::wSub = NULL;
wchar_t* SOUtils::wDefaultText = NULL;
bool SOUtils::s_pending = false;

void SOUtils::pedirTextoAsync(std::string titulo, std::string subtitulo, std::string textoPorDefecto,
							  SOUtilsKeyboardCallback callback, void* userData)
{
	if (s_pending) {
		return; 
	}

	int lenTitulo = MultiByteToWideChar(CP_ACP, 0, titulo.c_str(), -1, NULL, 0);
	int lenSub    = MultiByteToWideChar(CP_ACP, 0, subtitulo.c_str(), -1, NULL, 0);
	
	// 1. Calcular el tamanyo del nuevo texto por defecto
	int lenDefault = MultiByteToWideChar(CP_ACP, 0, textoPorDefecto.c_str(), -1, NULL, 0);

	if (wTitulo != NULL) { delete[] wTitulo; wTitulo = NULL; }
	if (wSub != NULL)    { delete[] wSub;    wSub = NULL; }
    
	// Variable estatica o miembro que debes declarar en tu clase (ej: wchar_t* SOUtils::wDefaultText = NULL;)
	if (wDefaultText != NULL) { delete[] wDefaultText; wDefaultText = NULL; }

	if (lenTitulo > 0){
		wTitulo = new (std::nothrow) wchar_t[lenTitulo];
		if (wTitulo != NULL) MultiByteToWideChar(CP_ACP, 0, titulo.c_str(), -1, wTitulo, lenTitulo);
	}
	
	if (lenSub > 0){
		wSub = new (std::nothrow) wchar_t[lenSub];
		if (wSub != NULL) MultiByteToWideChar(CP_ACP, 0, subtitulo.c_str(), -1, wSub, lenSub);
	}

	// 2. Convertir el texto por defecto a wchar_t
	if (lenDefault > 0){
		wDefaultText = new (std::nothrow) wchar_t[lenDefault];
		if (wDefaultText != NULL) MultiByteToWideChar(CP_ACP, 0, textoPorDefecto.c_str(), -1, wDefaultText, lenDefault);
	}
	
	s_pending = true;
	s_callback = callback;
	s_userData = userData;

	// 3. Pasamos 'wDefaultText' en lugar de L""
	// El comportamiento nativo seleccionara todo este texto automaticamente al abrir la ventana.
	XShowKeyboardUI(0, 
	                VKBD_DEFAULT, 
	                wDefaultText ? wDefaultText : L"", 
	                wTitulo, 
	                wSub, 
	                s_buffer, 
	                512, 
	                &s_overlapped);
}

void SOUtils::updateKeyboard()
{
	if (!s_pending) return;
	if (!XHasOverlappedIoCompleted(&s_overlapped)) return;

	// 1. CAMBIO DE ESTADO: La operacion asincrona ha terminado oficialmente.
	s_pending = false;

	// 2. LIBERACION DE MEMORIA: Ya podemos destruir los buffers unicode de forma segura
	// porque el sistema operativo ya ha cerrado la interfaz del teclado.
	if (wTitulo != NULL) {
		delete[] wTitulo;
		wTitulo = NULL;
	}
	if (wSub != NULL) {
		delete[] wSub;
		wSub = NULL;
	}

	std::string texto;
	if (XGetOverlappedExtendedError(&s_overlapped) == ERROR_SUCCESS)
	{
		int tam = WideCharToMultiByte(CP_ACP, 0, s_buffer, -1, NULL, 0, NULL, NULL);
		
		if (tam > 1) 
		{
			// 3. EVITAR DESBORDAMIENTO: Redimensionamos incluyendo el espacio para el caracter nulo '\0'.
			texto.resize(tam); 
			
			// La API escribe de forma segura dentro de los limites asignados del string
			WideCharToMultiByte(CP_ACP, 0, s_buffer, -1, &texto[0], tam, NULL, NULL);
			
			// 4. AJUSTE DE TAMANYO: Eliminamos el '\0' sobrante del final para que std::string
			// gestione su longitud real de manera interna y correcta.
			texto.resize(tam - 1); 
		}
	}

	// 5. NOTIFICACION: Ejecutamos el callback pasando el texto limpio.
	if (s_callback) {
		s_callback(texto, s_userData);
	}
}

#else

void SOUtils::pedirTextoAsync(std::string titulo, std::string subtitulo, std::string textoPorDefecto,
							  SOUtilsKeyboardCallback callback, void* userData)
{
	// No implementado en esta plataforma
}

void SOUtils::updateKeyboard()
{
	// No implementado en esta plataforma
}

#endif
