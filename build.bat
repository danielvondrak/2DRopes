@echo off
REM TODO: building with win32 requires vcvarsall.bat x86. Can we do this all with one exe?
set BUILD_PATH=build

rem -MTd - Prevents linking to the C Runtime Library DLL. Instead linked to static(?)
rem -nologo- Removes C/C++ logo info from cl
rem -Eha- - Removes error handling functionality from cpp
rem -Od - No optimizations. should only be used in debug
rem -Oi - Using intrisics where available.
set COMPILER_OPTS=-MTd -nologo -fp:fast -Gm- -GR- -EHa- -Od -Oi
set WARNING_OPTS=-WX -W4 -wd4201 -wd4100 -wd4189 -wd4390 -wd4505
set VARS= -DSLOW=1 -DINTERNAL=1 -DWIN32=1
set OUTPUT_OPTS=/Fm%BUILD_PATH%/Win32App.map /Fe%BUILD_PATH%/Win32App.exe /Fo%BUILD_PATH%/Win32App.obj /Fd%BUILD_PATH%/vc140.pdb
set LINKER_OPTS=-incremental:no -opt:ref gdi32.lib user32.lib winmm.lib
set DLL_OUTPUT_OPTS=/Fm%BUILD_PATH%/DV.map /Fe%BUILD_PATH%/DV.dll /Fo%BUILD_PATH%/DV.obj /Fd%BUILD_PATH%/DV.pdb
set DEBUG_OPTS=-FC -Zi

echo =========== cleaning ===========

rmdir /S /Q %BUILD_PATH%
mkdir %BUILD_PATH%

echo =========== building ===========
REM Optimization switches /O2 /Oi /fp:fast
echo WAITING FOR PDB > %BUILD_PATH%/lock.tmp
cl %COMPILER_OPTS% %WARNING_OPTS% %VARS% %DEBUG_OPTS% src\DV.cpp %DLL_OUTPUT_OPTS% /LD /link -incremental:no /EXPORT:GameGetSoundSamples /EXPORT:GameUpdateAndRender
del %BUILD_PATH%\lock.tmp
cl %COMPILER_OPTS% %WARNING_OPTS% %VARS% %DEBUG_OPTS% src\Win32App.cpp %OUTPUT_OPTS% /link %LINKER_OPTS%
echo =========== done ===========
