========================================================================
    WIN32 APPLICATION : modernscratch Project Overview
========================================================================

This is a testbed for various libraries and experiments.

modernscratch.cpp
    This is the main application source file.

The solution can be built using cmake, example:

mkdir VS11
pushd VS11
cmake -G "Visual Studio 11" -DBOOST_ROOT:string="<path>\boost_1_53_0" -DOPENSSL_ROOT_DIR:string="<path>\openssl" ..
popd
