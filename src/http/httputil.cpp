#include <http/httputil.h>
#include <const/constant.h>

#define DEBUG_NET(msg, ...) { \
    char buffer[256]; \
    int err = WSAGetLastError(); \
    sprintf(buffer, "NET_DEBUG: " msg " | LastError: %d\n", ##__VA_ARGS__, err); \
    OutputDebugStringA(buffer); \
}

//static const char* USERAGENT = "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:74.0) Gecko/20100101 Firefox/74.0";
static const char* USERAGENT = "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:45.0) Gecko/20100101 Firefox/45.0";

#ifdef _XBOX
mbedtls_entropy_context CurlClient::entropy;
mbedtls_ctr_drbg_context CurlClient::ctr_drbg;
#endif

volatile long CurlClient::g_abortScrapping;

CurlClient::CurlClient(){
}

CurlClient::~CurlClient(){
}

void CurlClient::init(){
	int err = 0;

	#ifdef _XBOX
		// 1. Comprobar si XNet ya está corriendo (vía una función simple)
		XNetStartupParams xsp;
		memset(&xsp, 0, sizeof(xsp));
		xsp.cfgSizeOfStruct = sizeof(XNetStartupParams);
		xsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY; 
		err = XNetStartup(&xsp);
			
		// Esperar a que la interfaz tenga una IP válida y estado activo
		XNADDR xnAddr;
		DWORD dwStatus;
		do {
			dwStatus = XNetGetTitleXnAddr(&xnAddr);
			OutputDebugStringA("Esperando configuración de red...\n");
			Sleep(200);
		} while (dwStatus == XNET_GET_XNADDR_PENDING || dwStatus == XNET_GET_XNADDR_NONE);
	#endif

		WORD wVersionRequested;
		WSADATA wsaData;
		wVersionRequested = MAKEWORD( 1, 1 );
		err = WSAStartup( wVersionRequested, &wsaData );
		if ( err != 0 ){
			// initialization failed
			return;
		}
 
		if (LOBYTE( wsaData.wVersion ) != 1 || HIBYTE( wsaData.wVersion ) != 1 ){
			WSACleanup( );
			return; 
		}

	#ifdef _XBOX
		// La IP está en xnAddr.ina.s_addr
		char ipStr[64];
		sprintf(ipStr, "IP de mi Xbox: %d.%d.%d.%d\n", 
				xnAddr.ina.S_un.S_un_b.s_b1, xnAddr.ina.S_un.S_un_b.s_b2, 
				xnAddr.ina.S_un.S_un_b.s_b3, xnAddr.ina.S_un.S_un_b.s_b4);
		OutputDebugStringA(ipStr);

		mbedtls_entropy_init(&entropy);

		// Añadimos nuestra fuente de la Xbox con prioridad fuerte
		mbedtls_entropy_add_source(&entropy, CurlClient::xbox360_entropy_source, NULL, 
									32, // Valor manual en lugar de MBEDTLS_ENTROPY_MIN_PLATFORM
									MBEDTLS_ENTROPY_SOURCE_STRONG);

		// Es vital "sembrar" el generador
		mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)"XBOX360", 7);
	#endif

	curl_global_init(CURL_GLOBAL_DEFAULT);
}

void CurlClient::close(){
	// 1. Limpiar cURL
	curl_global_cleanup();

	#ifdef _XBOX
	// 2. Limpiar mbedTLS
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_entropy_free(&entropy);
	#endif

	// 3. Cerrar Winsock
	WSACleanup();

	#ifdef _XBOX
    // 4. Cerrar XNet
		XNetCleanup();
	#endif
}

// Función principal de descarga
bool CurlClient::fetchUrl(const std::string& url, std::string& outResponse, float* progressPtr) {
    CURL *curl = curl_easy_init();
    if (!curl) return false;

    ProgressData pData;
    pData.progressVar = progressPtr;
    if (progressPtr) *progressPtr = 0.0f;

    outResponse.clear();

	#ifdef NET_DEBUG
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, CurlClient::debug_callback);
	#endif

    // Configuración básica
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        
    // SSL: Ignorar para evitar problemas con certificados en Xbox
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // Callback para los datos
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outResponse);

    // Callback para el progreso
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, ProgressCallback);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &pData);

    // Opciones adicionales
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USERAGENT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L); // 15 segundos

	// callback para llamar fuera a internet
	#ifdef _XBOX
	curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION, curl_sockopt_callback);
	curl_easy_setopt(curl, CURLOPT_SOCKOPTDATA, NULL); // Se podria pasar this
	#endif


    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

	if(res != CURLE_OK) {
		char errbuf[CURL_ERROR_SIZE];
		sprintf(errbuf, "Error curl: %s\n", curl_easy_strerror(res));
		OutputDebugStringA(errbuf);
	}

    return (res == CURLE_OK);
}

bool CurlClient::postUrl(const std::string& url, const std::string& postData,const std::string& user_agent, std::string& outResponse, float* progressPtr) {
	 CURL *curl = curl_easy_init();
    if (!curl) return false;

    ProgressData pData;
    pData.progressVar = progressPtr;
    if (progressPtr) *progressPtr = 0.0f;

    outResponse.clear();

	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
	headers = curl_slist_append(headers, "Accept: */*");
	headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.5");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	#ifdef NET_DEBUG
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, CurlClient::debug_callback);
	#endif

    // Configuración básica
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

	curl_easy_setopt(curl, CURLOPT_POST, 1);
	if (!postData.empty()) {
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    }

    // SSL: Ignorar para evitar problemas con certificados en Xbox
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // Callback para los datos
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outResponse);

    // Callback para el progreso
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, ProgressCallback);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &pData);

    // Opciones adicionales
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L); // 15 segundos

	// callback para llamar fuera a internet
	#ifdef _XBOX
	curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION, curl_sockopt_callback);
	curl_easy_setopt(curl, CURLOPT_SOCKOPTDATA, NULL); // Se podria pasar this
	#endif

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

	if(res != CURLE_OK) {
		char errbuf[CURL_ERROR_SIZE];
		sprintf(errbuf, "Error curl: %s\n", curl_easy_strerror(res));
		OutputDebugStringA(errbuf);
	}
	
	if (headers) curl_slist_free_all(headers);

    return (res == CURLE_OK);
}

// Función principal de descarga
bool CurlClient::postUrl(const std::string& url, const std::string& postData, std::string& outResponse, float* progressPtr) {
	return postUrl(url, postData, USERAGENT, outResponse, progressPtr);
}

// Añade esto a los métodos públicos de tu clase
bool CurlClient::fetchFile(const std::string& url, const std::string& localPath, float* progressPtr) {
	CURL *curl = curl_easy_init();
	if (!curl) return false;

	// Abrimos el archivo en modo binario para escritura
	FILE* fp = fopen(localPath.c_str(), "wb");
	if (!fp) {
		curl_easy_cleanup(curl);
		return false;
	}

	ProgressData pData;
	pData.progressVar = progressPtr;
	if (progressPtr) *progressPtr = 0.0f;

	// Configuración de la URL y SSL
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	// Callback para escribir directamente al archivo
	// Usamos el callback estándar de cURL para FILE*
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL); // NULL usa fwrite por defecto
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

	// Configuración de progreso
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, ProgressCallback);
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &pData);

	curl_easy_setopt(curl, CURLOPT_USERAGENT, USERAGENT);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Importante: seguir redirecciones de imágenes
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L); // 15 segundos

	// callback para llamar fuera a internet
	#ifdef _XBOX
	curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION, curl_sockopt_callback);
	curl_easy_setopt(curl, CURLOPT_SOCKOPTDATA, NULL); // Podrías pasar 'this' si es una clase
	#endif

	CURLcode res = curl_easy_perform(curl);

	fclose(fp);
	curl_easy_cleanup(curl);

	// Si la descarga falló, borramos el archivo parcial para no dejar basura
	if (res != CURLE_OK) {
		remove(localPath.c_str());
		return false;
	}
	return true;
}

std::string CurlClient::escape(const std::string& text) {
	CURL *curl = curl_easy_init();
	std::string escapedStr = "";
	if (curl) {
		// curl_easy_escape devuelve un char* que debe ser liberado con curl_free
		char *output = curl_easy_escape(curl, text.c_str(), (int)text.length());
		if (output) {
			escapedStr = output;
			curl_free(output);
		}
		curl_easy_cleanup(curl);
	}
	return escapedStr;
}

#ifdef _XBOX
	// Callback de entropía usando la API nativa de Xbox 360
	int CurlClient::xbox360_entropy_source(void *data, unsigned char *output, size_t len, size_t *olen) {
		// XNetRandom devuelve 0 si falla o la cantidad de bytes generados
		// En el XDK, suele llenar el buffer directamente.
		XNetRandom(output, (UINT)len);
		*olen = len;
		return 0;
	}
#endif

// Callback estático para recibir datos
std::size_t CurlClient::WriteCallback(void *contents, std::size_t size, std::size_t nmemb, void *userp) {
    size_t totalSize = size * nmemb;
    ((std::string*)userp)->append((char*)contents, totalSize);
    return totalSize;
}

// Callback estático para el progreso
int CurlClient::ProgressCallback(void* clientp, double dltotal, double dlnow, double ultotal, double ulnow) {
	// En VS2010 leemos el flag con Interlocked
    if (InterlockedExchangeAdd(&g_abortScrapping, 0) == 1) {
        return 1; // Esto fuerza a CURLcode a ser CURLE_ABORTED_BY_CALLBACK
    }
	
	ProgressData* p = (ProgressData*)clientp;
    if (p && p->progressVar && dltotal > 0) {
        *(p->progressVar) = (float)(dlnow / dltotal);
    }
    return 0;
}

// Esta función se ejecuta después de socket() pero antes de connect()
int CurlClient::curl_sockopt_callback(void *clientp, curl_socket_t curlfd, curlsocktype purpose) {
	DWORD bypass = 1;
	// Aplicamos el parche mágico de Xbox 360
	if (setsockopt(curlfd, SOL_SOCKET, 0x5801, (char*)&bypass, sizeof(bypass)) != 0) {
		OutputDebugStringA("Error aplicando bypass en socket de cURL\n");
	}
	return CURL_SOCKOPT_OK;
}

int CurlClient::debug_callback(CURL *handle, curl_infotype type,
                          char *data, size_t size, void *userptr) {
    (void)handle; (void)userptr;
    
    // Solo nos interesan los textos informativos y cabeceras
    if(type == CURLINFO_TEXT || type == CURLINFO_HEADER_IN || type == CURLINFO_HEADER_OUT) {
        char buffer[1024];
        if(size > 1023) size = 1023;
        memcpy(buffer, data, size);
        buffer[size] = '\0';
        
        // Enviarlo al panel de salida de Visual Studio
        OutputDebugStringA("CURL-DEBUG: ");
        OutputDebugStringA(buffer);
    }
    return 0;
}