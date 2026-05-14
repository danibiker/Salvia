set PATH=C:\Program Files (x86)\Microsoft Xbox 360 SDK\bin\win32;%PATH%
set XDK_PATH=C:\Program Files (x86)\Microsoft Xbox 360 SDK
:: Limpia el path de inclusión y pon el de Xbox primero
set INCLUDE=C:\Program Files (x86)\Microsoft Xbox 360 SDK\include\xbox
:: Limpia las librerías y pon las de Xbox
set LIB=C:\Program Files (x86)\Microsoft Xbox 360 SDK\lib\xbox
:: Asegura que no se usen las librerías de sistema de Windows
set EXTERNAL_INCLUDE=
#set INCLUDE=%XDK_PATH%\include\xbox;%INCLUDE%
set LIB=%XDK_PATH%\lib\xbox;%LIB%

set CURL_CFLAG_EXTRAS=/DHTTP_ONLY /Oi /Ot /fp:fast /FIxtl.h /FIwinsockx.h /DHAVE_GETHOSTBYNAME /DHAVE_STRUCT_HOSTENT /DCURL_DISABLE_MEMDEBUG /U_WIN32_WINNT /D_WIN32_WINNT=0x0500 /D"CURL_DISABLE_TCP_KEEPALIVE=1" /DHAVE_WSAIOCTL=0 /D"CURL_DISABLE_GETENV=1" /D"HAVE_FORMATMESSAGE=0" /DUSE_MBEDTLS /DHAVE_MBEDTLS_NET_FREE /I"..\..\mbedtls\include" /DPIX_NOT_SUPPORTED

set CURL_CFLAG_EXTRAS_RELEASE=/GL %CURL_CFLAG_EXTRAS%

set MACHINE=PPCBE

rem MODO RELEASE
nmake /f Makefile.vc mode=static VC=10 ENABLE_IDN=no ENABLE_IPV6=no RTLIBCFG=static ENABLE_SSPI=no ENABLE_WINSSL=no DEBUG=no CUSTOM_CFLAGS="%CURL_CFLAG_EXTRAS_RELEASE%"

rem MODO DEBUG
nmake /f Makefile.vc mode=static VC=10 ENABLE_IDN=no ENABLE_IPV6=no RTLIBCFG=static ENABLE_SSPI=no ENABLE_WINSSL=no DEBUG=yes CUSTOM_CFLAGS="%CURL_CFLAG_EXTRAS%"