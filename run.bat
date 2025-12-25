@echo off

mkdir build
copy ".\external\lib\*.dll"  ".\build\"

:: set enviroment vars and requred stuff for the msvc compiler
call "D:\Programming\Software\msvc\setup_x64.bat"

set CFLAGS=/Zi /EHsc /D_AMD64_ /fp:fast /W4 /MD /nologo /utf-8 /std:clatest /arch:AVX
:: /SUBSYSTEM:CONSOLE , /SUB-SYSTEM:WINDOWS 
set L_FLAGS=/SUBSYSTEM:CONSOLE
set SRC=..\Main.c ..\src\util.c ..\src\arena.c ..\src\base_graphics.c ..\src\simple_font.c ..\src\font.c ..\src\todo.c ..\src\client.c ..\external\src\glad.c ..\external\src\cJSON.c
set INCLUDE_DIRS=/I..\include /I..\external\include\
set LIBRARY_DIRS=/LIBPATH:..\external\lib\
set LIBRARIES=opengl32.lib glfw3.lib glew32.lib UxTheme.lib Dwmapi.lib user32.lib gdi32.lib shell32.lib kernel32.lib

if "%1"=="" (
    echo Usage: run.bat [rel|dbg]
    exit /b 1
)

if "%1"=="rel" (
    echo Building the project...
    pushd .\build
    cl %CFLAGS% /O2 %INCLUDE_DIRS% %SRC% /link %LIBRARY_DIRS% %LIBRARIES% %L_FLAGS%
    if errorlevel 1 (
        goto :build_failed
    )
    echo Build successful. Running...
    .\Main.exe
    goto :build_success
)

if "%1"=="dbg" (
    echo Building the project with debugging symbols...
    pushd .\build
    cl %CFLAGS% /fsanitize=address %INCLUDE_DIRS% %SRC% /link %LIBRARY_DIRS% %LIBRARIES% %L_FLAGS%
    if errorlevel 1 (
        goto :build_failed
    )
    echo Debug build successful. Launching debugger...
    start "" "devenv.exe" .\Main.exe
    goto :build_success
)

echo Unknown command: %1
exit /b 1

:build_failed
echo Build failed!
popd
exit /b 1

:build_success
popd
exit /b 0
