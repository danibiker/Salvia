@echo off
for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\Microsoft\VisualStudio\SxS\VS7" /v "10.0" 2^>nul ^| find "10.0"') do set "VS100COMNTOOLS=%%b"
if not defined VS100COMNTOOLS for /f "tokens=2*" %%a in ('reg query "HKLM\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS\VS7" /v "10.0" 2^>nul ^| find "10.0"') do set "VS100COMNTOOLS=%%b"
if not defined VS100COMNTOOLS echo ERROR: VS2010 no encontrado & exit /b 1
call "%VS100COMNTOOLS%\..\..\VC\bin\vcvars32.bat"

:: --- CONFIGURACIÓN ---
set PLATFORM="Win32"
set PLATFORM_XBOX="Xbox 360"
set CONFIG=Release
if /i "%~1"=="d" set CONFIG=Debug
:: Cambiado a /v:q (Quiet) y añadido /noconlog para evitar banners
set MS_OPTS=/t:Rebuild /p:Platform=%PLATFORM% /v:q /nologo /clp:NoSummary
set MS_OPTS_360=/t:Rebuild /p:Platform=%PLATFORM_XBOX% /v:q /nologo /clp:NoSummary

	setlocal enabledelayedexpansion
	:: Lista de entradas: NOMBRE|RUTA_AL_SLN
	set NAME[0]=beetle-pce
	set SLN[0]=beetle-pce\beetle-pce-libretro.sln
	set PLAT[0]=Both

	set NAME[1]=beetle-pce-fast
	set SLN[1]=beetle-pce-fast\beetle-pce-fast-libretro.sln
	set PLAT[1]=Both

	set NAME[2]=beetle-supergrafx
	set SLN[2]=beetle-supergrafx\msvc\beetle-supergrafx.sln
	set PLAT[2]=Both

	set NAME[3]=dosbox-pure
	set SLN[3]=dosbox-pure\vs2010\dosbox-pure.sln
	set PLAT[3]=Both

	set NAME[4]=gambatte
	set SLN[4]=gambatte\gambatte-libretro.sln
	set PLAT[4]=Both

	set NAME[5]=genesis-plus-gx
	set SLN[5]=genesis-plus-gx\Genesis-Plus-GX.sln
	set PLAT[5]=Both

	set NAME[6]=nestopia
	set SLN[6]=nestopia\nestopia.sln
	set PLAT[6]=Both

	set NAME[7]=picodrive
	set SLN[7]=picodrive\picodrive.sln
	set PLAT[7]=Both

	set NAME[8]=prboom
	set SLN[8]=prboom\msvc2010\prboom_libretro.sln
	set PLAT[8]=Both

	set NAME[9]=snes9x
	set SLN[9]=snes9x\snes9x.sln
	set PLAT[9]=Both

	set NAME[10]=snes9x2010
	set SLN[10]=snes9x2010\snes9x2010.sln
	set PLAT[10]=Both

	set NAME[11]=tyrquake
	set SLN[11]=tyrquake\tyrquake\tyrquake.sln
	set PLAT[11]=Both

	set NAME[12]=vba-next
	set SLN[12]=vba-next\vba-next.sln
	set PLAT[12]=Both

	set NAME[13]=mame-2003-plus
	set SLN[13]=mame-2003-plus\mame2003_plus_libretro.sln
	set PLAT[13]=Both

	set NAME[14]=fbneo
	set SLN[14]=FBNeo\projectfiles\visualstudio-2010-libretro-360\fba_vs2010_libretro_360.sln
	set PLAT[14]=Both

	set NAME[15]=fbanext
	set SLN[15]=fbanext\projectfiles\visualstudio-2010-libretro-360\fba_vs2010_libretro_360.sln
	set PLAT[15]=Xbox360

	set NAME[16]=pcsxr-360
	set SLN[16]=pcsxr-360\360\Xdk\pcsxr\pcsxr.sln
	set PLAT[16]=Xbox360

	set NAME[17]=3dox
	set SLN[17]=3dox\3dox_libretro.sln
	set PLAT[17]=Xbox360
	
	set NAME[18]=opera-libretro
	set SLN[18]=opera-libretro\opera\opera.sln
	set PLAT[18]=Win32
	
	set NAME[19]=beetle-psx-libretro
	set SLN[19]=beetle-psx-libretro\beetle-psx-libretro.sln
	set PLAT[19]=Win32

	for /l %%i in (0,1,19) do (
		set "_n=!NAME[%%i]!"
		set "_s=!SLN[%%i]!"
		set "_p=!PLAT[%%i]!"
		call :compile
	)
goto :done

:compile
	if /i "%_p%"=="Win32" goto :compile_win
	if /i "%_p%"=="Xbox360" goto :compile_xbox
	call :compile_win
	call :compile_xbox
	goto :eof

:compile_win
	echo [+] Procesando: %_n% [Win32]...
	msbuild %_s% %MS_OPTS% /p:Configuration=%CONFIG% > nul
	if errorlevel 1 (
		echo.
		echo [X] ERROR en: %_n% [Win32]. Reintentando con detalles para ver el fallo:
		echo.
		msbuild %_s% %MS_OPTS% /p:Configuration=%CONFIG% /v:m /nologo
		goto :error
	)
goto :eof

:compile_xbox
	echo [+] Procesando: %_n% [Xbox 360]...
	msbuild %_s% %MS_OPTS_360% /p:Configuration=%CONFIG% > nul
	if errorlevel 1 (
		echo.
		echo [X] ERROR en: %_n% [Xbox 360]. Reintentando con detalles para ver el fallo:
		echo.
		msbuild %_s% %MS_OPTS_360% /p:Configuration=%CONFIG% /v:m /nologo
		goto :error
	)
goto :eof

:done
	
	
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

