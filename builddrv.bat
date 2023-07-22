@echo off
cl.exe /nologo /D_M_AMD64 /D_WIN64 /D_AMD64_ /DWINNT=1 /D_WIN32_WINNT=0x0601 /DWINVER=0x0601 /DNDEBUG /permissive /std:c++20 /utf-8 /O2 /Zp8 /Gy /GS- /GR- /GF drvtest.cpp /link /release /NODEFAULTLIB /SUBSYSTEM:NATIVE /DRIVER /INTEGRITYCHECK /BASE:0x10000 /MACHINE:x64 /IGNORE:4281 /FORCE:MULTIPLE /ENTRY:DriverEntry /OUT:drvtest.sys ntoskrnl.lib
if not %errorlevel%==0 pause
csigntool.exe sign /ac /r abc /f drvtest.sys
if not %errorlevel%==0 pause

rc.exe /nologo drv.rc
if not %errorlevel%==0 pause

ml64.exe /nologo /c hook.asm
if not %errorlevel%==0 pause
cl.exe /nologo /D_M_AMD64 /D_WIN64 /D_AMD64_ /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /DNDEBUG /DUNICODE /std:c++latest /permissive /Zc:strictStrings- /utf-8 /WX /MP /O2 /Zp8 /Gy /GS- /GR- /EHs-c- /Gw /GL /GA /GF /guard:cf- /guard:ehcont- Source.cpp /link /RELEASE /LTCG /NODEFAULTLIB /SUBSYSTEM:WINDOWS /DYNAMICBASE:NO /OPT:REF /DEBUG:NONE /MACHINE:x64 /IGNORE:4281 /FORCE:MULTIPLE /WX /ENTRY:start /OUT:EldenRing.exe hook.obj msvcrt.lib kernel32.lib advapi32.lib user32.lib dxgi.lib d3d11.lib d3d12.lib dxguid.lib Uuid.Lib KernelBase.lib ntdll.lib drv.res
pause