@echo off
setlocal enabledelayedexpansion

:: --- CONFIGURACIÓN ---
set PROJECT="Salvia.vcxproj"
set PLATFORM="Xbox 360"
:: Cambiado a /v:q (Quiet) y añadido /noconlog para evitar banners
set MS_OPTS=/t:Rebuild /p:Platform=%PLATFORM% /v:q /nologo /clp:NoSummary

set CORES=Release Release_beetlepce Release_beetlepcefast Release_beetlepce_fx ^
Release_Gambatte Release_Nestopia Release_Snes9x Release_Snes9x_latest ^
Release_vbanext Release_picodrive Release_mame Release_finalburn Release_DosboxPure

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
