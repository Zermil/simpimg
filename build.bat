@echo off

REM Change this to your visual studio's 'vcvars64.bat' script path
set MSVC_PATH="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build"
set CXXFLAGS=/std:c++17 /EHsc /W4 /WX /Zl /wd4996 /wd4201 /nologo %*
set INCLUDES=/I"deps\GLEW\include" /I"deps\GLFW\include"
set LIBS="deps\GLFW\lib\glfw3.lib" "deps\GLEW\lib\glew32s.lib" opengl32.lib User32.lib Gdi32.lib Shell32.lib

call %MSVC_PATH%\vcvars64.bat

pushd %~dp0
if not exist .\build mkdir build
cl %CXXFLAGS% %INCLUDES% code\main.cpp /Fo:build\ /Fe:build\opengl.exe %LIBS% /link /NODEFAULTLIB:libcmt.lib
popd