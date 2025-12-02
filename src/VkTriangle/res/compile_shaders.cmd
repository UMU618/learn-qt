set GLSLC_BIN="%VCPKG_ROOT%\packages\shaderc_x64-windows\tools\shaderc\glslc.exe"
if not exist %GLSLC_BIN% (
    set GLSLC_BIN="%VCPKG_ROOT%\installed\x64-windows\tools\shaderc\glslc.exe"
)
if not exist %GLSLC_BIN% (
    echo GLSL compiler not found. Please ensure shaderc is installed via `vcpkg install shaderc`.
    pause
    exit /b 1
)

%GLSLC_BIN% color.vert -o color_vert.spv
%GLSLC_BIN% color.frag -o color_frag.spv
pause
