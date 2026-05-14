@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat" x86

nmake /f Makefile.vc mode=static clean
nmake /f Makefile.vc mode=static VC=10 ENABLE_IDN=no ENABLE_IPV6=no RTLIBCFG=static DEBUG=no

pause

