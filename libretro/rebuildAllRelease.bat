@echo off
for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\Microsoft\VisualStudio\SxS\VS7" /v "10.0" 2^>nul ^| find "10.0"') do set "VS100COMNTOOLS=%%b"
if not defined VS100COMNTOOLS for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS\VS7" /v "10.0" 2^>nul ^| find "10.0"') do set "VS100COMNTOOLS=%%b"
if not defined VS100COMNTOOLS echo ERROR: VS2010 no encontrado & exit /b 1
call "%VS100COMNTOOLS%\..\..\VC\bin\vcvars32.bat"

:: --- CONFIGURACIÓN ---
set PLATFORM="Win32"
set PLATFORM_XBOX="Xbox 360"
set CONFIG=Release
:: Cambiado a /v:q (Quiet) y añadido /noconlog para evitar banners
set MS_OPTS=/t:Rebuild /p:Platform=%PLATFORM% /v:q /nologo /clp:NoSummary
set MS_OPTS_360=/t:Rebuild /p:Platform=%PLATFORM_XBOX% /v:q /nologo /clp:NoSummary

echo =======================================================
echo Compilando %PLATFORM% (Modo Silencioso)
echo =======================================================
	
	setlocal enabledelayedexpansion
	:: Lista de entradas: NOMBRE|RUTA_AL_SLN
	set NAME[0]=beetle-pce
	set SLN[0]=beetle-pce\beetle-pce-libretro.sln

	set NAME[1]=beetle-pce-fast
	set SLN[1]=beetle-pce-fast\beetle-pce-fast-libretro.sln

	set NAME[2]=beetle-supergrafx
	set SLN[2]=beetle-supergrafx\msvc\beetle-supergrafx.sln

	set NAME[3]=dosbox-pure
	set SLN[3]=dosbox-pure\vs2010\dosbox-pure.sln

	set NAME[4]=gambatte
	set SLN[4]=gambatte\gambatte-libretro.sln

	set NAME[5]=genesis-plus-gx
	set SLN[5]=genesis-plus-gx\Genesis-Plus-GX.sln

	set NAME[6]=nestopia
	set SLN[6]=nestopia\nestopia.sln

	set NAME[7]=picodrive
	set SLN[7]=picodrive\picodrive.sln

	set NAME[8]=prboom
	set SLN[8]=prboom\msvc2010\prboom_libretro.sln

	set NAME[9]=snes9x
	set SLN[9]=snes9x\snes9x.sln

	set NAME[10]=snes9x2010
	set SLN[10]=snes9x2010\snes9x2010.sln

	set NAME[11]=tyrquake
	set SLN[11]=tyrquake\tyrquake\tyrquake.sln

	set NAME[12]=vba-next
	set SLN[12]=vba-next\vba-next.sln

	set NAME[13]=mame-2003-plus
	set SLN[13]=mame-2003-plus\mame2003_plus_libretro.sln

	set NAME[14]=fbneo
	set SLN[14]=FBNeo\projectfiles\visualstudio-2010-libretro-360\fba_vs2010_libretro_360.sln
	
	::-----------------------------------------------------------
	::solo para xbox 360
	::-----------------------------------------------------------
	set NAME[15]=fbanext
	set SLN[15]=fbanext\projectfiles\visualstudio-2010-libretro-360\fba_vs2010_libretro_360.sln
	
	set NAME[16]=pcsxr-360
	set SLN[16]=pcsxr-360\360\Xdk\pcsxr\pcsxr.sln
	
	set NAME[17]=3dox
	set SLN[17]=3dox\3dox_libretro.sln

	::-----------------------------------------------------------

	for /l %%i in (0,1,14) do (
		echo [+] Procesando: !NAME[%%i]!...
		msbuild !SLN[%%i]! %MS_OPTS% /p:Configuration=%CONFIG% > nul
		
		if errorlevel 1 (
			echo.
			echo [X] ERROR en: %%C. Reintentando con detalles para ver el fallo:
			echo.
			:: Si falla, lo ejecutamos OTRA VEZ sin silenciar para que veas el error real
			msbuild !SLN[%%i]! %MS_OPTS% /p:Configuration=%CONFIG% /v:m /nologo
			goto :error
		)
	)
	
echo =======================================================
echo Compilando %PLATFORM_XBOX% (Modo Silencioso)
echo =======================================================	
	
	for /l %%i in (0,1,17) do (
		echo [+] Procesando: !NAME[%%i]!...
		msbuild !SLN[%%i]! %MS_OPTS_360% /p:Configuration=%CONFIG% > nul
		
		if errorlevel 1 (
			echo.
			echo [X] ERROR en: %%C. Reintentando con detalles para ver el fallo:
			echo.
			:: Si falla, lo ejecutamos OTRA VEZ sin silenciar para que veas el error real
			msbuild !SLN[%%i]! %MS_OPTS_360% /p:Configuration=%CONFIG% /v:m /nologo
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

