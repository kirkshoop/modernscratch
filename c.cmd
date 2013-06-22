@echo off

call \\vriska\shared\stlbuild.bat wcfb01-2013.05.05

setlocal

set RXREPOPATH=%1
shift
set LIBREPOPATH=%1
shift

if '%RXREPOPATH%' == '' (set RXREPOPATH=ext\Rx)
if '%LIBREPOPATH%' == '' (set LIBREPOPATH=ext\kirkshoop)

pushd C:\Users\kshoop\Desktop\source\modernscratch

CL.exe /Ic:\Users\kshoop\Desktop\source\modernscratch\packages\cpprestsdk.0.6.0.21\build\native\..\..\build\native\include\ -I%RXREPOPATH%\Rx\CPP\src -I%RXREPOPATH%\Ix\CPP\src -I%LIBREPOPATH%\libraries  /ZI /nologo /W4 /WX /Od /Oy- /D HAS_CPPRESTSDK /D RXCPP_FORCE_USE_VARIADIC_TEMPLATES /D WIN32 /D _DEBUG /D _WINDOWS /D _UNICODE /D UNICODE /Gm /EHsc /RTC1 /MDd /GS /fp:precise /Zc:wchar_t /Zc:forScope /Fo"Debug\\" /Fd"modernscratch.pdb" /Gd /TP /analyze- /errorReport:queue modernscratch.cpp

REM cl -EHsc -I%RXREPOPATH%\Rx\CPP\src -I%RXREPOPATH%\Ix\CPP\src -I%LIBREPOPATH%\libraries -DUNICODE -D_UNICODE -DRXCPP_FORCE_USE_VARIADIC_TEMPLATES -ZI -Fd"modernscratch.pdb" modernscratch.cpp

popd
endlocal