@echo off

ctime -begin ui_panel.ctm

mkdir build
copy ".\external\lib\*.dll"  ".\build\"

:: set enviroment vars and requred stuff for the msvc compiler
call "D:\Programming\Software\msvc\setup_x64.bat"

set EXE_NAME=ui_panel
set CFLAGS=/EHsc /fp:fast /W4 /MD /nologo /utf-8 /std:clatest /arch:AVX /Fe:%EXE_NAME%.exe
:: /SUBSYSTEM:CONSOLE , /SUB-SYSTEM:WINDOWS 
set L_FLAGS=/SUBSYSTEM:CONSOLE
set SRC=..\Main.c 
::..\src\util.c ..\src\arena.c ..\src\base_graphics.c ..\src\simple_font.c ..\src\font.c ..\src\todo.c ..\external\src\glad.c ..\external\src\cJSON.c
set INCLUDE_DIRS=/I..\include /I..\external\include\
set LIBRARY_DIRS=/LIBPATH:..\external\lib\
set LIBRARIES=opengl32.lib glfw3.lib sqlite3.lib glew32.lib UxTheme.lib Dwmapi.lib user32.lib gdi32.lib shell32.lib kernel32.lib

if "%1"=="" (
    echo Usage: run.bat [rel|dbg]
    exit /b 1
)

if "%1"=="rel" (
    echo Building the project...
    pushd .\build
    cl %CFLAGS% /O2 %INCLUDE_DIRS% %SRC% /link %LIBRARY_DIRS% %LIBRARIES% %L_FLAGS%
    if errorlevel 1 (
        echo -----------------------------------------------------------------
        echo Build failed!
        echo -----------------------------------------------------------------
        goto :build_failed
    )
    echo -----------------------------------------------------------------
    echo Release Build Successful. 
    echo Build time :
    ctime -end ../ui_panel.ctm
    D:\Programming\Software\cloc\cloc.exe --quiet --hide-rate ..\src\ ..\Main.c
    echo Running...
    echo -----------------------------------------------------------------

    .\%EXE_NAME%.exe
    goto :build_success
)

if "%1"=="dbg" (
    echo Building the project with debugging symbols...
    pushd .\build
    cl /Zi %CFLAGS% /fsanitize=address %INCLUDE_DIRS% %SRC% /link %LIBRARY_DIRS% %LIBRARIES% %L_FLAGS% /DEBUG
    if errorlevel 1 (
        echo -----------------------------------------------------------------
        echo Build failed !
        goto :build_failed
        echo -----------------------------------------------------------------
    )
    echo -----------------------------------------------------------------
    echo Debug Build Successful. 
    echo Build time :
    ctime -end ../ui_panel.ctm    
    echo Launching debugger...
    echo -----------------------------------------------------------------
    start "" "raddbg.exe" .\%EXE_NAME%.exe
    @REM start "" "devenv.exe" .\%EXE_NAME%.exe
    goto :build_success
)

echo Unknown command: %1
exit /b 1

:build_failed
popd
exit /b 1

:build_success
popd
exit /b 0
