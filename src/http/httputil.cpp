#include <http/httputil.h>
#include <const/constant.h>

//static const char* USERAGENT = "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:74.0) Gecko/20100101 Firefox/74.0";
static const char* USERAGENT = "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:45.0) Gecko/20100101 Firefox/45.0";
static const char* COOKIE_JAR_FILE = "cookies.txt";

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
		// 1. Comprobar si XNet ya esta corriendo (via una funcion simple)
		XNetStartupParams xsp;
		memset(&xsp, 0, sizeof(xsp));
		xsp.cfgSizeOfStruct = sizeof(XNetStartupParams);
		xsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY; 
		err = XNetStartup(&xsp);
			
		// Esperar a que la interfaz tenga una IP valida y estado activo
		XNADDR xnAddr;
		DWORD dwStatus;
		do {
			dwStatus = XNetGetTitleXnAddr(&xnAddr);
			LOG_DEBUG("Esperando configuracion de red...\n");
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
		// La IP esta en xnAddr.ina.s_addr
		char ipStr[64];
		sprintf(ipStr, "IP de mi Xbox: %d.%d.%d.%d\n", 
				xnAddr.ina.S_un.S_un_b.s_b1, xnAddr.ina.S_un.S_un_b.s_b2, 
				xnAddr.ina.S_un.S_un_b.s_b3, xnAddr.ina.S_un.S_un_b.s_b4);
		LOG_DEBUG(ipStr);

		mbedtls_entropy_init(&entropy);

		// Anyadimos nuestra fuente de la Xbox con prioridad fuerte
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

// Funcion principal de descarga
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

    // Configuracian basica
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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 10 segundos

	if (!cookie.empty()){
		// Inyecta la cadena de cookies directamente (separadas por punto y coma)
		curl_easy_setopt(curl, CURLOPT_COOKIE, this->cookie);
	} else {
		//Lee las cookies de este archivo para la peticion actual
		curl_easy_setopt(curl, CURLOPT_COOKIEFILE, COOKIE_JAR_FILE);	
	}
	// Guarda las cookies nuevas/actualizadas aqui al finalizar
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, COOKIE_JAR_FILE);
	

	// callback para llamar fuera a internet
	#ifdef _XBOX
	curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION, curl_sockopt_callback);
	curl_easy_setopt(curl, CURLOPT_SOCKOPTDATA, NULL); // Se podria pasar this
	#endif

	#ifdef NET_DEBUG
	LOG_DEBUG("Downloading url: %s", url.c_str());
	#endif

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

	#ifdef NET_DEBUG
	if(res != CURLE_OK) {
		LOG_DEBUG("get_KO: %s\n", curl_easy_strerror(res));
	} else {
		LOG_DEBUG("get_ok curl: %s\n", url.c_str());
	}
	#endif

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

    // Configuracion basica
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

	#ifdef NET_DEBUG
	if(res != CURLE_OK) {
		LOG_DEBUG("post_KO curl: %s\n", curl_easy_strerror(res));
	} else {
		LOG_DEBUG("post_ok curl: %s\n", url.c_str());
	}
	#endif
	
	if (headers) curl_slist_free_all(headers);

    return (res == CURLE_OK);
}

// Funcion principal de descarga
bool CurlClient::postUrl(const std::string& url, const std::string& postData, std::string& outResponse, float* progressPtr) {
	return postUrl(url, postData, USERAGENT, outResponse, progressPtr);
}

// Anyade esto a los metodos publicos de tu clase
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

	// Configuracion de la URL y SSL
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	// Callback para escribir directamente al archivo
	// Usamos el callback estandar de cURL para FILE*
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL); // NULL usa fwrite por defecto
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

	// Configuracion de progreso
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, ProgressCallback);
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &pData);

	curl_easy_setopt(curl, CURLOPT_USERAGENT, USERAGENT);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Importante: seguir redirecciones de imagenes
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L); // 15 segundos
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L); // Treat HTTP 4xx/5xx as curl error

	// callback para llamar fuera a internet
	#ifdef _XBOX
	curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION, curl_sockopt_callback);
	curl_easy_setopt(curl, CURLOPT_SOCKOPTDATA, NULL); // Podrias pasar 'this' si es una clase
	#endif

	CURLcode res = curl_easy_perform(curl);

	fclose(fp);
	curl_easy_cleanup(curl);

	// Si la descarga falla, borramos el archivo parcial para no dejar basura
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
	// Callback de entropia usando la API nativa de Xbox 360
	int CurlClient::xbox360_entropy_source(void *data, unsigned char *output, size_t len, size_t *olen) {
		// XNetRandom devuelve 0 si falla o la cantidad de bytes generados
		// En el XDK, suele llenar el buffer directamente.
		XNetRandom(output, (UINT)len);
		*olen = len;
		return 0;
	}
#endif

// Callback estatico para recibir datos
std::size_t CurlClient::WriteCallback(void *contents, std::size_t size, std::size_t nmemb, void *userp) {
    /* NULL check defensivo: si por alguna razon userp llega NULL,
     * derreferenciarlo causaria access violation reading 0x00000010
     * (el offset de los miembros internos de std::string en MSVC2010).
     * Hemos visto crashes con esta firma; aqui devolvemos 0 para que
     * curl marque la transferencia como fallida en vez de crashear. */
    if (!userp) {
        LOG_ERROR("WriteCallback: userp is NULL, aborting transfer");
        return 0;
    }
    if (!contents || size == 0 || nmemb == 0) {
        return 0;
    }
    size_t totalSize = size * nmemb;
    ((std::string*)userp)->append((char*)contents, totalSize);
    return totalSize;
}

// Callback estatico para el progreso
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

// Esta funcion se ejecuta despues de socket() pero antes de connect()
int CurlClient::curl_sockopt_callback(void *clientp, curl_socket_t curlfd, curlsocktype purpose) {
	DWORD bypass = 1;
	// Aplicamos el parche magico de Xbox 360
	if (setsockopt(curlfd, SOL_SOCKET, 0x5801, (char*)&bypass, sizeof(bypass)) != 0) {
		LOG_DEBUG("Error aplicando bypass en socket de cURL\n");
	}
	return CURL_SOCKOPT_OK;
}

int CurlClient::debug_callback(CURL *handle, curl_infotype type,
                          char *data, size_t size, void *userptr) {
    (void)handle; (void)userptr;
    
    if(type == CURLINFO_TEXT || type == CURLINFO_HEADER_IN || type == CURLINFO_HEADER_OUT) {
        // En Xbox 360, mejor imprimir trozos pequenyos para no saturar el bus de debug
        char buffer[128]; 
        size_t copySize = (size > 128) ? 128 : size;
        
        memcpy(buffer, data, copySize);
        buffer[copySize - 1] = '\0';
        
        // Solo imprimir si hay contenido real para no saturar el canal de comunicacion XDK
        if (copySize > 0) {
			LOG_DEBUG("CURL: %s", buffer);
        }
    }
    return 0;
}

#ifdef _XBOX
#include <winsockx.h>
#ifdef __cplusplus
extern "C" {
#endif

	//Funcion definida en curl
	int Curl_gethostname(char *name, size_t namelen);

	//Hacemos un wrapper a la funcion de curl
	int gethostname(char *name, size_t namelen){
		return Curl_gethostname(name, namelen);
	}

	struct hostent* gethostbyaddr(const char* addr, int len, int type) {
		static struct hostent h;
		static char host_name[256];
		static unsigned long address_list;
		static char* h_addr_ptrs[2];
		static char* host_aliases[1] = { NULL };

		if (type != AF_INET || len != sizeof(struct in_addr)) {
			return NULL;
		}

		struct in_addr* in = (struct in_addr*)addr;
		char ip_str[32];

		// Reemplazo nativo a inet_ntoa usando la estructura de sockets de Windows/Xbox
		sprintf_s(ip_str, sizeof(ip_str), "%d.%d.%d.%d",
				  in->S_un.S_un_b.s_b1,
				  in->S_un.S_un_b.s_b2,
				  in->S_un.S_un_b.s_b3,
				  in->S_un.S_un_b.s_b4);
    
		// Por defecto, usamos la IP como respaldo si cURL no resuelve nada
		strcpy_s(host_name, sizeof(host_name), ip_str);

		// Consulta de DNS mediante cURL
		CURL* curl = curl_easy_init();
		if (curl) {
			curl_easy_setopt(curl, CURLOPT_URL, ip_str);
			curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); 
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L); 

			if (curl_easy_perform(curl) == CURLE_OK) {
				char* resolved_url = NULL;
				if (curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &resolved_url) == CURLE_OK && resolved_url) {
					char* clean_host = resolved_url;
					if (strncmp(clean_host, "http://", 7) == 0) clean_host += 7;
					if (strncmp(clean_host, "https://", 8) == 0) clean_host += 8;
                
					strcpy_s(host_name, sizeof(host_name), clean_host);
				}
			}
			curl_easy_cleanup(curl);
		}

		// Estructurar la respuesta para el motor de TyrQuake
		address_list = in->s_addr;
		h_addr_ptrs[0] = (char*)&address_list;
		h_addr_ptrs[1] = NULL;

		h.h_name = host_name;
		h.h_aliases = host_aliases;
		h.h_addrtype = AF_INET;
		h.h_length = sizeof(struct in_addr);
		h.h_addr_list = h_addr_ptrs;

		return &h;
	}

	// 1. Solución para inet_aton
    // Convierte una cadena "X.X.X.X" en una estructura de dirección de red (in_addr)
    int inet_aton(const char *cp, struct in_addr *inp) {
        unsigned int b1, b2, b3, b4;
        
        // Escaneamos los 4 segmentos de la IP de forma segura
        if (sscanf_s(cp, "%u.%u.%u.%u", &b1, &b2, &b3, &b4) != 4) {
            return 0; // Error de parseo
        }
        
        // Validamos que ningún byte exceda el límite de 255
        if (b1 > 255 || b2 > 255 || b3 > 255 || b4 > 255) {
            return 0;
        }

        // Reconstruimos los bytes en la estructura de destino
        inp->S_un.S_un_b.s_b1 = (unsigned char)b1;
        inp->S_un.S_un_b.s_b2 = (unsigned char)b2;
        inp->S_un.S_un_b.s_b3 = (unsigned char)b3;
        inp->S_un.S_un_b.s_b4 = (unsigned char)b4;
        
        return 1; // Éxito
    }

    // 2. Solución para inet_ntoa
    // Convierte una estructura in_addr en texto legible "X.X.X.X"
    char* inet_ntoa(struct in_addr in) {
        // Usamos una variable estática local compartiendo el comportamiento del original thread-unsafe
        static char buffer[32]; 
        
        sprintf_s(buffer, sizeof(buffer), "%d.%d.%d.%d",
                  in.S_un.S_un_b.s_b1,
                  in.S_un.S_un_b.s_b2,
                  in.S_un.S_un_b.s_b3,
                  in.S_un.S_un_b.s_b4);
                  
        return buffer;
    }

    // 3. Solución para inet_pton
    // Equivalente seguro multiespecificación, TyrQuake solo la usa para AF_INET (IPv4)
    int inet_pton(int af, const char *src, void *dst) {
        if (af != AF_INET) {
            return -1; // Xbox 360 no da soporte nativo a IPv6 (AF_INET6) en este entorno
        }
        
        if (src == NULL || dst == NULL) {
            return 0;
        }

        // Delegamos de forma limpia en nuestra función inet_aton ya definida arriba
        return inet_aton(src, (struct in_addr*)dst);
    }
#ifdef __cplusplus
}
#endif
#endif