@echo off
setlocal

set RXREPOPATH=%1
shift
set LIBREPOPATH=%1
shift

if '%RXREPOPATH%' == '' (set RXREPOPATH=ext\Rx)
if '%LIBREPOPATH%' == '' (set LIBREPOPATH=ext\kirkshoop)

pushd C:\Users\kshoop\Desktop\source\modernscratch

call \\vriska\shared\stlbuild.bat wcfb01-2013.03.29-v25

cl -EHsc -I%RXREPOPATH%\Rx\CPP\src -I%RXREPOPATH%\Ix\CPP\src -I%LIBREPOPATH%\libraries -DUNICODE -D_UNICODE -DDELAY_ON_WORKER_THREAD -ZI -Fd"testbench.pdb" modernscratch.cpp

popd
endlocal