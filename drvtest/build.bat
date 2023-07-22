@echo off
cl.exe /nologo /D_M_AMD64 /D_WIN64 /D_AMD64_ /DWINNT=1 /D_WIN32_WINNT=0x0601 /DWINVER=0x0601 /DNDEBUG /permissive /std:c++20 /utf-8 /O2 /Zp8 /Gy /GS- /GR- /GF drvtest.cpp /link /release /NODEFAULTLIB /SUBSYSTEM:NATIVE /DRIVER /INTEGRITYCHECK /BASE:0x10000 /MACHINE:x64 /IGNORE:4281 /FORCE:MULTIPLE /ENTRY:DriverEntry /OUT:drvtest.sys ntoskrnl.lib
csigntool.exe sign /ac /r abc /f drvtest.sys
pause