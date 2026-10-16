@echo off

ctime -begin ui_panel.ctm

mkdir build >nul 2>&1
copy ".\external\lib\*.dll"  ".\build\" >nul 2>&1

:: set enviroment vars and requred stuff for the msvc compiler
call "D:\Programming\Software\msvc\setup_x64.bat"

set EXE_NAME=ui_panel
:: /Bt /d2cgsummary 
set CFLAGS=/fp:fast /W4 /MD /nologo /utf-8 /std:clatest /arch:AVX 
:: /SUBSYSTEM:CONSOLE /SUB-SYSTEM:WINDOWS /INCREMENTAL /time /Fe:%EXE_NAME%.exe
set L_FLAGS=/SUBSYSTEM:CONSOLE
set SRC=..\main.c 
::..\src\util.c ..\src\arena.c ..\src\base_graphics.c ..\src\simple_font.c ..\src\font.c ..\src\todo.c ..\external\src\glad.c ..\external\src\cJSON.c
set INCLUDE_DIRS=/I..\include /I..\external\include\
set LIBRARY_DIRS=/LIBPATH:..\external\lib\
set LIBRARIES=opengl32.lib glfw3.lib glew32.lib UxTheme.lib Dwmapi.lib user32.lib gdi32.lib shell32.lib kernel32.lib
:: sqlite3.lib

if "%1"=="" (
    echo Usage:Specify a flag run.bat dbg ^| rel ^| all
    exit /b 1
)

if "%1"=="rel" (
    echo Building the project...
    pushd .\build
    call :build_release %EXE_NAME%
    if errorlevel 1 goto :build_failed
    echo -----------------------------------------------------------------
    echo Release Build Successful. 
    echo Build time :
    ctime -end ../ui_panel.ctm
    :: call :cloc
    echo Running...
    echo -----------------------------------------------------------------
    .\%EXE_NAME%.exe
    goto :build_success
)

if "%1"=="dbg" (
    echo Building the project with debugging symbols...
    pushd .\build
    call :build_debug %EXE_NAME%
    if errorlevel 1 goto :build_failed
    echo -----------------------------------------------------------------
    echo Debug Build Successful. 
    echo Build time :
    ctime -end ../ui_panel.ctm    
    echo Launching debugger...
    echo -----------------------------------------------------------------
    start "" "raddbg.exe" .\%EXE_NAME%.exe
    :: start "" "devenv.exe" .\build\%EXE_NAME%.exe
    goto :build_success
)

if "%1"=="all" (
    echo Building..
    pushd .\build
    echo Building release [%EXE_NAME%.exe]...
    call :build_release %EXE_NAME%
    if errorlevel 1 goto :build_failed
    echo -----------------------------------------------------------------
    echo Building debug [%EXE_NAME%_dbg.exe]...
    call :build_debug %EXE_NAME%_dbg
    if errorlevel 1 goto :build_failed
    echo -----------------------------------------------------------------
    echo Build Successful. 
    echo Build time :
    ctime -end ../ui_panel.ctm
    :: call :cloc
    echo Running release build...
    echo -----------------------------------------------------------------
    .\%EXE_NAME%.exe
    goto :build_success
)

echo Unknown command: %1
exit /b 1

:build_release
cl %CFLAGS% /O2 %INCLUDE_DIRS% %SRC% /Fe:%~1.exe /link %LIBRARY_DIRS% %LIBRARIES% %L_FLAGS%
if errorlevel 1 (
    echo -----------------------------------------------------------------
    echo Build failed!
    echo -----------------------------------------------------------------
    exit /b 1
)
exit /b 0

:build_debug
cl /Zi %CFLAGS% /fsanitize=address %INCLUDE_DIRS% %SRC% /Fe:%~1.exe /link %LIBRARY_DIRS% %LIBRARIES% %L_FLAGS% /DEBUG
if errorlevel 1 (
    echo -----------------------------------------------------------------
    echo Build failed !
    echo -----------------------------------------------------------------
    exit /b 1
)
exit /b 0

:cloc
D:\Programming\Software\cloc\cloc.exe --quiet --hide-rate ..\src\ ..\Main.c | more +2
exit /b 0

:build_failed
popd
exit /b 1

:build_success
popd
exit /b 0
