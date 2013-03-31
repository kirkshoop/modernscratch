@echo off
pushd C:\Users\kshoop\Desktop\source\modernscratch
call \\vriska\shared\stlbuild.bat wcfb01-2013.03.29-v25
cl -EHsc -IC:\Users\kshoop\Desktop\source\Rx\Rx\CPP\src -IC:\Users\kshoop\Desktop\source\Rx\Ix\CPP\src -IC:\Users\kshoop\Desktop\source\libraries -DUNICODE -D_UNICODE -DDELAY_ON_WORKER_THREAD -ZI -Fd"testbench.pdb" modernscratch.cpp
popd
