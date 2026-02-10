#pragma once

#include <string>
#include <vector>

#include <SDL.h>
#include <curl/curl.h>

#ifdef _XBOX
	#include <xtl.h>
	#include <WinSockX.h>
	#include <mbedtls/entropy.h>
	#include <mbedtls/ctr_drbg.h>
	typedef int socklen_t;
#endif

struct ProgressData {
    float* progressVar;
};

class CurlClient {
	public:
		CurlClient();
		~CurlClient();
		void init();
		void close();

		// Función principal de descarga
		bool fetchUrl(const std::string& url, std::string& outResponse, float* progressPtr);
		bool fetchFile(const std::string& url, const std::string& localPath, float* progressPtr);
		std::string escape(const std::string& text);
	private:
	#ifdef _XBOX
		static mbedtls_entropy_context entropy;
		static mbedtls_ctr_drbg_context ctr_drbg;
		// Callback de entropía usando la API nativa de Xbox 360
		static int xbox360_entropy_source(void *data, unsigned char *output, size_t len, size_t *olen);
	#endif

		// Callback estático para recibir datos
		static std::size_t __cdecl WriteCallback(void *contents, std::size_t size, std::size_t nmemb, void *userp);
		// Callback estático para el progreso
		static int __cdecl ProgressCallback(void* clientp, double dltotal, double dlnow, double ultotal, double ulnow);
		// Esta función se ejecuta después de socket() pero antes de connect()
		static int __cdecl curl_sockopt_callback(void *clientp, curl_socket_t curlfd, curlsocktype purpose);
		// Callback para mostrar informacion de depuracion
		static int __cdecl debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr);
};

//typedef struct {
//	Uint32 host;			/* 32-bit IPv4 host address */
//	Uint16 port;			/* 16-bit protocol port */
//} IPaddress;
//
//struct TCPsocket {
//	int ready;
//	SOCKET channel;
//	IPaddress remoteAddress;
//	IPaddress localAddress;
//	int sflag;
//};
//	void testDns() {
//		XNDNS* pTest = NULL;
//		HANDLE hDnsEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
//    
//		if (hDnsEvent == NULL) return;
//
//		// Lanzar la consulta
//		if (XNetDnsLookup("portquiz.net", hDnsEvent, &pTest) == 0) {
//        
//			// El pequeño Sleep que descubrimos que estabiliza el iStatus
//			Sleep(200); 
//
//			// Esperar si todavía está pendiente
//			if (pTest->iStatus == 0x103) { 
//				WaitForSingleObject(hDnsEvent, 5000);
//			}
//
//			if (pTest->iStatus == 0) {
//				// La IP está en pTest->aina[0]
//				// Usamos los bytes de la estructura S_un para formatear la cadena
//				char ipMsg[128];
//				sprintf(ipMsg, "DNS OK! IP Resuelta: %d.%d.%d.%d\n", 
//						pTest->aina[0].S_un.S_un_b.s_b1, 
//						pTest->aina[0].S_un.S_un_b.s_b2, 
//						pTest->aina[0].S_un.S_un_b.s_b3, 
//						pTest->aina[0].S_un.S_un_b.s_b4);
//            
//				OutputDebugStringA(ipMsg);
//			} else {
//				char msg[64];
//				sprintf(msg, "Error DNS Final: 0x%X\n", pTest->iStatus);
//				OutputDebugStringA(msg);
//			}
//
//			// Limpieza de la estructura DNS
//			XNetDnsRelease(pTest);
//		}
//
//		// Cerrar el evento
//		CloseHandle(hDnsEvent);
//	}
//
//	XNDNS* XResolveHost(const char *host) 
//	{
//		// hostname not found in cache.
//		// do a DNS lookup
//		WSAEVENT hEvent = WSACreateEvent();
//		XNDNS* pDns		  = NULL;
//		INT err = XNetDnsLookup(host, hEvent, &pDns);
//		WaitForSingleObject( (HANDLE)hEvent, INFINITE);
//		if( pDns && pDns->iStatus == 0 )
//		{
//			//DNS lookup succeeded
//			WSACloseEvent(hEvent);
//			return pDns;
//		}
//		if (pDns)
//		{
//			XNetDnsRelease(pDns);
//		}
//
//		WSACloseEvent(hEvent);
//		return 0;
//	}
//
//	void testDns2(std::string hostname) {
//		XNDNS* pTest = NULL;
//		pTest = XResolveHost(hostname.c_str());
//
//		if(pTest && pTest->iStatus == 0){
//			char ipMsg[128];
//				sprintf(ipMsg, "DNS OK! IP Resuelta: %d.%d.%d.%d\n", 
//						pTest->aina[0].S_un.S_un_b.s_b1, 
//						pTest->aina[0].S_un.S_un_b.s_b2, 
//						pTest->aina[0].S_un.S_un_b.s_b3, 
//						pTest->aina[0].S_un.S_un_b.s_b4);
//            
//				OutputDebugStringA(ipMsg);
//			
//		} else {
//			char msg[64];
//			sprintf(msg, "Error DNS Final: 0x%X\n", pTest->iStatus);
//			OutputDebugStringA(msg);
//		}
//		XNetDnsRelease(pTest);
//	}
//
//	void testLocalConnect() {
//		// 1. Crear el socket
//		SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//		if (s == INVALID_SOCKET) return;
//
//		// 2. Registrar la ruta en XNet (Vital para comunicación PC-Xbox)
//		IN_ADDR ipPC;
//		ipPC.s_addr = inet_addr("141.94.139.59");
//		XNetInAddrToXnAddr(ipPC, NULL, NULL); 
//
//		// 3. Poner el socket en modo NO BLOQUEANTE
//		unsigned long iMode = 1;
//		ioctlsocket(s, FIONBIO, &iMode);
//
//		sockaddr_in target;
//		target.sin_family = AF_INET;
//		target.sin_port = htons(80);
//		target.sin_addr = ipPC;
//
//		// 4. Intentar conectar
//		// Como es no bloqueante, devolverá SOCKET_ERROR y el error será WSAEWOULDBLOCK
//		connect(s, (struct sockaddr*)&target, sizeof(target));
//
//		// 5. Configurar el timeout con select()
//		fd_set writeSet;
//		FD_ZERO(&writeSet);
//		FD_SET(s, &writeSet);
//
//		timeval tv;
//		tv.tv_sec = 3;  // 3 segundos de timeout
//		tv.tv_usec = 0;
//
//		OutputDebugStringA("Intentando conectar al PC (141.94.139.59:80)...\n");
//
//		// select() esperará hasta que el socket esté listo para escribir (conectado)
//		int total = select(0, NULL, &writeSet, NULL, &tv);
//
//		if (total > 0 && FD_ISSET(s, &writeSet)) {
//			OutputDebugStringA("¡CONEXIÓN LOCAL EXITOSA!\n");
//		} else {
//			int err = WSAGetLastError();
//			char msg[64];
//			sprintf(msg, "Fallo o Timeout. Error: %d\n", err);
//			OutputDebugStringA(msg);
//		}
//
//		// 6. Volver a poner el socket en modo bloqueante si lo vas a reusar
//		iMode = 0;
//		ioctlsocket(s, FIONBIO, &iMode);
//
//		closesocket(s);
//	}
//
//	void testLocalConnect2() {
//		SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//    
//		// 1. OMITIR XNetInAddrToXnAddr si te da 10022. 
//		// En su lugar, vamos a forzar una pequeña espera antes del connect.
//		Sleep(1000); 
//
//		sockaddr_in target;
//		target.sin_family = AF_INET;
//		target.sin_port = htons(80);
//		target.sin_addr.s_addr = inet_addr("141.94.139.59");
//
//		// 2. Usar CONNECT BLOQUEANTE pero con un Timeout de Recibo
//		// Esto es más compatible con el stack sencillo de la 360
//		int timeout = 10000; // 3 segundos
//		setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
//		setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
//
//		OutputDebugStringA("Intentando conexión directa (Bloqueante)...\n");
//    
//		// Si el connect falla aquí con 10060, es problema de FIREWALL o RUTA
//		if (connect(s, (struct sockaddr*)&target, sizeof(target)) == SOCKET_ERROR) {
//			int err = WSAGetLastError();
//			char msg[64];
//			sprintf(msg, "Error de conexion: %d\n", err);
//			OutputDebugStringA(msg);
//		} else {
//			OutputDebugStringA("¡CONEXIÓN LOCAL EXITOSA!\n");
//		}
//
//		closesocket(s);
//	}
//
//	void testInternetConnect() {
//		XNADDR xnAddr;
//		DWORD dwStatus;
//    
//		// 1. Espera activa hasta que el estado sea ONLINE
//		// XNET_GET_XNADDR_PENDING (0x01)
//		// XNET_GET_XNADDR_NONE (0x00)
//		// Necesitamos que dwStatus tenga flags de conectividad (normalmente 0x08 o similar)
//		do {
//			dwStatus = XNetGetTitleXnAddr(&xnAddr);
//			OutputDebugStringA("Esperando Gateway...\n");
//			Sleep(500);
//		} while (dwStatus == XNET_GET_XNADDR_PENDING || xnAddr.ina.s_addr == 0);
//
//		// 2. IMPORTANTE: Registrar la IP de destino en el stack de seguridad
//		// Aunque sea una IP de internet, esto le dice a la Xbox que "confíe" en esta ruta
//		IN_ADDR targetIP;
//		targetIP.s_addr = inet_addr("35.180.139.74"); // PortQuiz
//		
//		// 1. Notificar a la capa XNet que vamos a conectar a esta IP
//		// Esto intenta establecer la asociación de seguridad (SA)
//		int result = XNetConnect(targetIP); 
//
//		if (result != 0) {
//			int err = WSAGetLastError();
//			char msg[64];
//			sprintf(msg, "XNetConnect falló con código: %d", result);
//			OutputDebugStringA(msg);
//		} else {
//			int err = WSAGetLastError();
//			char msg[64];
//			sprintf(msg, "XNetConnect iniciado correctamente");
//			OutputDebugStringA(msg);
//		}
//
//		// Esta llamada es mágica en el XDK: "Pre-autoriza" la conexión
//		//XNetInAddrToXnAddr(targetIP, NULL, NULL); 
//
//		SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//    
//		// 3. Aumentar el buffer de envío/recepción (ayuda con la estabilidad en 360)
//		int bufSize = 64 * 1024;
//		setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char*)&bufSize, sizeof(bufSize));
//
//		sockaddr_in target;
//		target.sin_family = AF_INET;
//		target.sin_port = htons(80); 
//		target.sin_addr = targetIP;
//
//		// 4. Timeout corto para el connect (para no esperar 10s si va a fallar)
//		unsigned long iMode = 1; 
//		ioctlsocket(s, FIONBIO, &iMode); // No bloqueante
//
//		OutputDebugStringA("Lanzando connect tras autorizacion XNet...\n");
//		connect(s, (struct sockaddr*)&target, sizeof(target));
//
//		fd_set writeSet;
//		FD_ZERO(&writeSet);
//		FD_SET(s, &writeSet);
//		timeval tv = {5, 0}; // 5 segundos
//
//		if (select(0, NULL, &writeSet, NULL, &tv) > 0) {
//			OutputDebugStringA("¡CONEXIÓN EXITOSA!\n");
//		} else {
//			int err = WSAGetLastError();
//			char msg[64];
//			sprintf(msg, "Sigue fallando. Error: %d. Status XNet: 0x%X\n", err, dwStatus);
//			OutputDebugStringA(msg);
//		}
//
//		closesocket(s);
//	}
//
//	void testFinalLocal() {
//		SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//    
//		// Modo NO bloqueante
//		unsigned long iMode = 1;
//		ioctlsocket(s, FIONBIO, &iMode);
//
//		sockaddr_in target;
//		target.sin_family = AF_INET;
//		target.sin_port = htons(80); // Puerto 80 del PC
//		target.sin_addr.s_addr = inet_addr("192.168.0.1");
//
//		connect(s, (struct sockaddr*)&target, sizeof(target));
//
//		fd_set writeSet;
//		FD_ZERO(&writeSet);
//		FD_SET(s, &writeSet);
//		timeval tv = {2, 0}; // 2 segundos son suficientes para red local
//
//		if (select(0, NULL, &writeSet, NULL, &tv) > 0) {
//			OutputDebugStringA("¡RED LOCAL FUNCIONA!\n");
//		} else {
//			OutputDebugStringA("Fallo local. Revisa el Firewall del PC.\n");
//		}
//		closesocket(s);
//	}
//
//
//
//	void testLocal2(){
//		IPaddress ip;
//		ip.host = inet_addr("35.180.139.74");
//		ip.port = htons(8); // Puerto 80 del PC
//
//		// ... supongamos que ya tienes ip.host e ip.port asignados ...
//		char debugBuffer[256];
//
//		// 1. Mostrar el Puerto (en hexadecimal para ver el orden de bytes)
//		// Si htons funciona en PowerPC, 80 debe ser 0x0050
//		sprintf(debugBuffer, "DEBUG: Puerto Red (hex): 0x%04X\n", ip.port);
//		OutputDebugStringA(debugBuffer);
//
//		// 2. Mostrar la IP (en hexadecimal)
//		// Para "192.168.0.1" debería ser 0xC0A80001
//		sprintf(debugBuffer, "DEBUG: IP Red (hex): 0x%08X\n", ip.host);
//		OutputDebugStringA(debugBuffer);
//
//		// 3. Mostrar la IP en formato legible (opcional)
//		unsigned char *bytes = (unsigned char*)&ip.host;
//		sprintf(debugBuffer, "DEBUG: IP Decodificada: %d.%d.%d.%d\n", 
//				bytes[0], bytes[1], bytes[2], bytes[3]);
//		OutputDebugStringA(debugBuffer);
//
//		SDLNet_TCP_Open(&ip);
//	}
//
//	SOCKET CrearSocketInseguro() {
//		SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//		if (s != INVALID_SOCKET) {
//			DWORD bypass = 1;
//			setsockopt(s, SOL_SOCKET, 0x5801, (char*)&bypass, sizeof(bypass));
//		}
//		return s;
//	}
//
///* Open a TCP network socket
//   If 'remote' is NULL, this creates a local server socket on the given port,
//   otherwise a TCP connection to the remote host and port is attempted.
//   The newly created socket is returned, or NULL if there was an error.
//*/
//	TCPsocket* SDLNet_TCP_Open(IPaddress *ip)
//	{
//		TCPsocket *sock;
//		struct sockaddr_in sock_addr;
//
//		/* Allocate a TCP socket structure */
//		sock = (TCPsocket *)malloc(sizeof(TCPsocket));
//		if ( sock == NULL ) {
//			OutputDebugStringA("Out of memory");
//			goto error_return;
//		}
//
//		sock->channel = INVALID_SOCKET;
//
//		/* Open the socket */
//		sock->channel = CrearSocketInseguro();
//		if ( sock->channel == INVALID_SOCKET ) {
//			OutputDebugStringA("Couldn't create socket");
//			goto error_return;
//		}
//
//		/* Connect to remote, or bind locally, as appropriate */
//		if ( (ip->host != INADDR_NONE) && (ip->host != INADDR_ANY) ) {
//
//		// #########  Connecting to remote
//	
//			memset(&sock_addr, 0, sizeof(sock_addr));
//			sock_addr.sin_family = AF_INET;
//			sock_addr.sin_addr.s_addr = ip->host;
//			sock_addr.sin_port = ip->port;
//
//			/*IN_ADDR targetIP;
//			targetIP.s_addr = ip->host; // PortQuiz
//			int result = XNetConnect(targetIP); 
//
//			if (result != 0) {
//				int err = WSAGetLastError();
//				char msg[64];
//				sprintf(msg, "XNetConnect falló con código: %d", result);
//				OutputDebugStringA(msg);
//			} else {
//				int err = WSAGetLastError();
//				char msg[64];
//				sprintf(msg, "XNetConnect iniciado correctamente");
//				OutputDebugStringA(msg);
//			}*/
//
//			/* Connect to the remote host */
//			if ( connect(sock->channel, (struct sockaddr *)&sock_addr,
//					sizeof(sock_addr)) == SOCKET_ERROR ) {
//				DEBUG_NET("Fallo connect a %08X:%d", ip->host, ip->port);
//				goto error_return;
//			} else {
//				DEBUG_NET("EXITO connect a %08X:%d!!!!!!", ip->host, ip->port);
//			}
//			sock->sflag = 0;
//		} else {
//
//		// ##########  Binding locally
//
//			memset(&sock_addr, 0, sizeof(sock_addr));
//			sock_addr.sin_family = AF_INET;
//			sock_addr.sin_addr.s_addr = INADDR_ANY;
//			sock_addr.sin_port = ip->port;
//
//	/*
//	 * Windows gets bad mojo with SO_REUSEADDR:
//	 * http://www.devolution.com/pipermail/sdl/2005-September/070491.html
//	 *   --ryan.
//	 */
//	#ifndef WIN32
//			/* allow local address reuse */
//			{ int yes = 1;
//				setsockopt(sock->channel, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));
//			}
//	#endif
//
//			/* Bind the socket for listening */
//			if ( bind(sock->channel, (struct sockaddr *)&sock_addr,
//					sizeof(sock_addr)) == SOCKET_ERROR ) {
//				OutputDebugStringA("Couldn't bind to local port");
//				goto error_return;
//			}
//			if ( listen(sock->channel, 5) == SOCKET_ERROR ) {
//				OutputDebugStringA("Couldn't listen to local port");
//				goto error_return;
//			}
//
//			/* Set the socket to non-blocking mode for accept() */
//	#if defined(__BEOS__) && defined(SO_NONBLOCK)
//			/* On BeOS r5 there is O_NONBLOCK but it's for files only */
//			{
//				long b = 1;
//				setsockopt(sock->channel, SOL_SOCKET, SO_NONBLOCK, &b, sizeof(b));
//			}
//	#elif defined(O_NONBLOCK)
//			{
//				fcntl(sock->channel, F_SETFL, O_NONBLOCK);
//			}
//	#elif defined(WIN32)
//			{
//				unsigned long mode = 1;
//				ioctlsocket (sock->channel, FIONBIO, &mode);
//			}
//	#elif defined(__OS2__)
//			{
//				int dontblock = 1;
//				ioctl(sock->channel, FIONBIO, &dontblock);
//			}
//	#else
//	//#warning How do we set non-blocking mode on other operating systems?
//	#endif
//			sock->sflag = 1;
//		}
//		sock->ready = 0;
//
//	#ifdef TCP_NODELAY
//		/* Set the nodelay TCP option for real-time games */
//		{ int yes = 1;
//		setsockopt(sock->channel, IPPROTO_TCP, TCP_NODELAY, (char*)&yes, sizeof(yes));
//		}
//	#endif /* TCP_NODELAY */
//
//		/* Fill in the channel host address */
//		sock->remoteAddress.host = sock_addr.sin_addr.s_addr;
//		sock->remoteAddress.port = sock_addr.sin_port;
//
//		/* The socket is ready */
//		return(sock);
//
//	error_return:
//		SDLNet_TCP_Close(sock);
//		return(NULL);
//	}
//
//	/* Close a TCP network socket */
//	void SDLNet_TCP_Close(TCPsocket *sock)
//	{
//		if ( sock != NULL ) {
//			if ( sock->channel != NULL && sock->channel != INVALID_SOCKET ) {
//				closesocket(sock->channel);
//			}
//			free(sock);
//		}
//	}

