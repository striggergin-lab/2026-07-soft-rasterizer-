@echo off
setlocal
taskkill /IM soft_rasterizer.exe /F >nul 2>nul
set MSBUILD="C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
if not exist %MSBUILD% (
  set MSBUILD="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe"
)
echo Using MSBuild: %MSBUILD%
%MSBUILD% "%~dp0soft_rasterizer.sln" /p:Configuration=Release /p:Platform=x64 /m
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo.
echo Build OK. Run:
echo   %~dp0x64\Release\soft_rasterizer.exe
