cd jansson-2.4
autoreconf -i
./configure
make
cd ..
mkdir picoc/libs
mkdir picoc/libs/include
cp -r jansson-2.4/src/.libs picoc/libs/lib
cp jansson-2.4/src/jansson.h picoc/libs/include
cd picoc
make -f MakefileSS

