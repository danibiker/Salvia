#pragma once

#include <string>
#include <vector>
#include <map>

#include <SDL.h>

// En Xbox 360 la red va por el XDK (xtl.h / WinSockX.h). Estas cabeceras deben ir
// ANTES que curl.h, y marcamos _WINSOCKAPI_ para que curl.h NO intente incluir
// <winsock2.h> (inexistente en el XDK; ver curl.h ~L77-85: solo lo incluye si no
// estan definidos _WINSOCKAPI_ / _WINSOCK_H).
#ifdef _XBOX
	#include <xtl.h>
	#include <WinSockX.h>
	#ifndef _WINSOCKAPI_
		#define _WINSOCKAPI_
	#endif
	typedef int socklen_t;

	// El XDK no define struct hostent (Xbox 360 no tiene gethostbyname/gethostbyaddr
	// nativos; se resuelve por XNetDnsLookup). Antes llegaba via el winsock2.h de curl,
	// que ahora bloqueamos con _WINSOCKAPI_. La definimos para los wrappers de
	// resolucion de httputil.cpp (usados por curl / tyrquake).
	#ifndef SALVIA_XBOX_HOSTENT_DEFINED
	#define SALVIA_XBOX_HOSTENT_DEFINED
	struct hostent {
		char*  h_name;
		char** h_aliases;
		short  h_addrtype;
		short  h_length;
		char** h_addr_list;
	};
	#endif
#endif

#include <curl/curl.h>

struct ProgressData {
    float* progressVar;
};

enum NETWORK_STATUS{NET_OK, NET_NONE, NET_ERROR_IP, NET_ERROR_WSA, NET_ERROR_WINSOCK, NET_MAX};

class CurlClient {
	public:
		CurlClient();
		~CurlClient();
		NETWORK_STATUS init();
		void close();

		static volatile long g_abortScrapping; 

		// Función principal de descarga
		bool fetchUrl(const std::string&, std::string&, float*);
		bool postUrl(const std::string&, const std::string&, std::string&, float*);
		bool postUrl(const std::string&, const std::string&, const std::string&, std::string&, float*);
		bool fetchFile(const std::string&, const std::string&, float*);
		std::string escape(const std::string&);

		void setCookie(const std::string &s){
			cookie = s;
		}

		void setHeaders(const std::map<std::string, std::string> &h){
			customHeaders = h;
		}

		void setSSLVersion(long v){
			sslVersion = v;
		}

		void setCipherList(const std::string &c){
			cipherList = c;
		}

		void setSessionIdCache(bool enabled){
			sessionIdCache = enabled;
		}

		void setHttpVersion(long v){
			httpVersion = v;
		}

		// Ultimo codigo de estado HTTP (200, 403, 503...) de la peticion mas reciente.
		// Imprescindible para medir bloqueos de Cloudflare: un 403/503 devuelve CURLE_OK.
		long getLastHttpCode() const {
			return lastHttpCode;
		}

		std::string getIp(){
			return ipStr;
		}

		NETWORK_STATUS getNetInitStat(){
			return netStat;
		}

	private:
		std::string cookie;
		std::map<std::string, std::string> customHeaders;
		long sslVersion;
		std::string cipherList;
		bool sessionIdCache;
		long httpVersion;
		long lastHttpCode;
		CURL* m_curl; // handle reutilizable: conserva cookies (en memoria) entre peticiones
		char ipStr[64];
		NETWORK_STATUS netStat;

		// Callback estático para recibir datos
		static std::size_t __cdecl WriteCallback(void *contents, std::size_t size, std::size_t nmemb, void *userp);
		// Callback estático para el progreso
		static int __cdecl ProgressCallback(void* clientp, double dltotal, double dlnow, double ultotal, double ulnow);
		// Esta función se ejecuta después de socket() pero antes de connect()
		static int __cdecl curl_sockopt_callback(void *clientp, curl_socket_t curlfd, curlsocktype purpose);
		// Callback para mostrar informacion de depuracion
		static int __cdecl debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr);
};