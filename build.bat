@echo off

ml64.exe /nologo /c hook.asm
if not %errorlevel%==0 pause
cl.exe /nologo /D_M_AMD64 /D_WIN64 /D_AMD64_ /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /DNDEBUG /DUNICODE /std:c++latest /permissive /Zc:strictStrings- /utf-8 /WX /MP /O2 /Zp8 /Gy /GS- /GR- /EHs-c- /Gw /GL /GA /GF /guard:cf- /guard:ehcont- Source.cpp /link /RELEASE /LTCG /NODEFAULTLIB /SUBSYSTEM:WINDOWS /DYNAMICBASE:NO /OPT:REF /DEBUG:NONE /MACHINE:x64 /IGNORE:4281 /FORCE:MULTIPLE /WX /ENTRY:start /OUT:EldenRing.exe hook.obj msvcrt.lib kernel32.lib advapi32.lib user32.lib dxgi.lib d3d11.lib d3d12.lib dxguid.lib Uuid.Lib KernelBase.lib ntdll.lib drv.res
pause