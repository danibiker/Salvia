@echo off
for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\Microsoft\VisualStudio\SxS\VS7" /v "10.0" 2^>nul ^| find "10.0"') do set "VS100COMNTOOLS=%%b"
if not defined VS100COMNTOOLS for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS\VS7" /v "10.0" 2^>nul ^| find "10.0"') do set "VS100COMNTOOLS=%%b"
if not defined VS100COMNTOOLS echo ERROR: VS2010 no encontrado & exit /b 1
call "%VS100COMNTOOLS%\..\..\VC\bin\vcvars32.bat"

:: --- CONFIGURACIÓN ---
set PLATFORM="Win32"
:: Cambiado a /v:q (Quiet) y añadido /noconlog para evitar banners
set MS_OPTS=/t:Rebuild /p:Platform=%PLATFORM% /v:q /nologo /clp:NoSummary /p:DeployOnBuild=false

set LIBS=wolfssl\IDE\XBOX360\wolfssl.vcxproj ^
curl\projects\Windows\VC10\lib\libcurl.vcxproj ^
minizip\minizip.vcxproj ^
zlib\zlib.vcxproj ^
rcheevos\rcheevos.vcxproj

echo =======================================================
echo Compilando %PLATFORM% (Modo Silencioso)
echo =======================================================

for %%C in (%LIBS%) do (
    echo [+] Procesando: %%C...
    
    :: Ejecutamos y mandamos la salida estándar a NUL, pero dejamos que los errores (2) pasen
    msbuild %%C %MS_OPTS% /p:Configuration=Release > nul
    
    if errorlevel 1 (
        echo.
        echo [X] ERROR en: %%C. Reintentando con detalles para ver el fallo:
        echo.
        :: Si falla, lo ejecutamos OTRA VEZ sin silenciar para que veas el error real
        msbuild %%C %MS_OPTS% /p:Configuration=Release /v:m /nologo
        goto :error
    )
)

set "URL=https://www.libsdl.org/projects/old/SDL_image/release/SDL_image-devel-1.2.12-VC.zip"
set "ZIP_NAME=SDL_image-devel-1.2.12-VC.zip"

echo Descargando archivo SDL_image...
curl -L -o "%ZIP_NAME%" "%URL%"

echo Descomprimiendo archivo SDL_image...
tar -xf "%ZIP_NAME%"

del "%ZIP_NAME%"


set "URL=https://sourceforge.net/projects/libsdl/files/SDL_ttf/2.0.11/SDL_ttf-devel-2.0.11-VC.zip"
set "ZIP_NAME=SDL_ttf-devel-2.0.11-VC.zip"

echo Descargando archivo SDL_ttf...
curl -L -o "%ZIP_NAME%" "%URL%"

echo Descomprimiendo archivo SDL_ttf...
tar -xf "%ZIP_NAME%"

del "%ZIP_NAME%"


set "URL=https://www.libsdl.org/release/SDL-devel-1.2.14-VC8.zip"
set "ZIP_NAME=SDL-devel-1.2.14-VC8.zip"

echo Descargando archivo SDL...
curl -L -o "%ZIP_NAME%" "%URL%"

echo Descomprimiendo archivo SDL...
tar -xf "%ZIP_NAME%"

del "%ZIP_NAME%"

echo.
echo =======================================================
echo [OK] TODAS LAS LIBRERIAS COMPLETADAS
echo =======================================================
pause
exit /b 0

:error
echo.
echo [!] Compilacion abortada.
pause
exit /b 1
