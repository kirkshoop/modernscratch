@echo off
setlocal

set RXREPOPATH=%1
shift
set LIBREPOPATH=%1
shift

if '%RXREPOPATH%' == '' (set RXREPOPATH=ext\Rx)
if '%LIBREPOPATH%' == '' (set LIBREPOPATH=ext\kirkshoop)

pushd C:\Users\kshoop\Desktop\source\modernscratch

call \\vriska\shared\stlbuild.bat wcfb01-2013.04.09

cl -EHsc -I%RXREPOPATH%\Rx\CPP\src -I%RXREPOPATH%\Ix\CPP\src -I%LIBREPOPATH%\libraries -DUNICODE -D_UNICODE -DRXCPP_FORCE_USE_VARIADIC_TEMPLATES -ZI -Fd"modernscratch.pdb" modernscratch.cpp

popd
endlocal