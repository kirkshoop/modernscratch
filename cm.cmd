pushd build

cmake -G "Visual Studio 11" -DBOOST_ROOT:string="C:\Users\kshoop\Desktop\source\boost_1_53_0" -DOPENSSL_ROOT_DIR:string="C:\Users\kshoop\Desktop\source\openssl\build" ..

popd