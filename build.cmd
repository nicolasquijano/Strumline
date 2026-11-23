@echo off
rem Builds Strumline with the Visual Studio Build Tools already on this machine (the same ones Schematic Amp Sim uses).
rem   build.cmd                 configure + build everything
rem   build.cmd target <name>   build one target (e.g. SasMidiProbe_VST3)
setlocal
set "VS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
cd /d "%~dp0"

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
if /i "%~1"=="target" (
  cmake --build build --target %~2 || exit /b 1
) else (
  cmake --build build || exit /b 1
)
endlocal
