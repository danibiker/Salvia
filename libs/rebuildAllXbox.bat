@echo off
for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\Microsoft\VisualStudio\SxS\VS7" /v "10.0" 2^>nul ^| find "10.0"') do set "VS100COMNTOOLS=%%b"
if not defined VS100COMNTOOLS for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS\VS7" /v "10.0" 2^>nul ^| find "10.0"') do set "VS100COMNTOOLS=%%b"
if not defined VS100COMNTOOLS echo ERROR: VS2010 no encontrado & exit /b 1
call "%VS100COMNTOOLS%\..\..\VC\bin\vcvars32.bat"

:: --- CONFIGURACIÓN ---
set PLATFORM="Xbox 360"
:: Cambiado a /v:q (Quiet) y añadido /noconlog para evitar banners
set MS_OPTS=/t:Rebuild /p:Platform=%PLATFORM% /v:q /nologo /clp:NoSummary /p:DeployOnBuild=false

set LIBS=SDL_image-1.2.12\libjpeg\jpeg.vcxproj ^
SDL_image-1.2.12\lpng1513\projects\vstudio\libpng\libpng.vcxproj ^
SDL_image-1.2.12\VisualC\SDL_image.vcxproj ^
SDL_ttf360\SDL_ttf360.vcxproj ^
libSDLx360\libSDLx360.vcxproj ^
wolfssl\IDE\XBOX360\wolfssl.vcxproj ^
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
