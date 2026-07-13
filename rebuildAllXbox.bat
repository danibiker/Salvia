@echo off
for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\Microsoft\VisualStudio\SxS\VS7" /v "10.0" 2^>nul ^| find "10.0"') do set "VS100COMNTOOLS=%%b"
if not defined VS100COMNTOOLS for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS\VS7" /v "10.0" 2^>nul ^| find "10.0"') do set "VS100COMNTOOLS=%%b"
if not defined VS100COMNTOOLS echo ERROR: VS2010 no encontrado & exit /b 1
call "%VS100COMNTOOLS%\..\..\VC\bin\vcvars32.bat"

:: --- CONFIGURACIÓN ---
set PROJECT="Salvia.vcxproj"
set PLATFORM="Xbox 360"
:: Cambiado a /v:q (Quiet) y añadido /noconlog para evitar banners
set MS_OPTS=/t:Rebuild /p:Platform=%PLATFORM% /v:q /nologo /clp:NoSummary /p:DeployOnBuild=false

set CORES=Release Release_beetlepce Release_beetlepcefast Release_beetlepce_fx ^
Release_Gambatte Release_Nestopia Release_Snes9x Release_Snes9x_latest ^
Release_vbanext Release_picodrive Release_prboom ReleaseTyrQuake Release_pcsxr_360 ^
Release_3DOX Release_DosboxPure Release_mame Release_fbanext Release_finalburn 

echo =======================================================
echo Compilando %PLATFORM% (Modo Silencioso)
echo =======================================================

for %%C in (%CORES%) do (
    echo [+] Procesando: %%C...
    
    :: Ejecutamos y mandamos la salida estándar a NUL, pero dejamos que los errores (2) pasen
    msbuild %PROJECT% %MS_OPTS% /p:Configuration=%%C > nul
    
    if errorlevel 1 (
        echo.
        echo [X] ERROR en: %%C. Reintentando con detalles para ver el fallo:
        echo.
        :: Si falla, lo ejecutamos OTRA VEZ sin silenciar para que veas el error real
        msbuild %PROJECT% /t:Build /p:Platform=%PLATFORM% /p:Configuration=%%C /v:m /nologo
        goto :error
    )
)

echo.
echo =======================================================
echo [OK] TODOS LOS NUCLEOS COMPLETADOS
echo =======================================================
pause
exit /b 0

:error
echo.
echo [!] Compilacion abortada.
pause
exit /b 1
