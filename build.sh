#!/bin/sh

if [ "$1" == "clean" ]; then
	rm -rf dependency
	rm -rf open-source
	rm -rf /opt/mips_kvs_deplib
	rm -rf _build
	exit
fi

rm -rf dependency
rm -rf open-source

rm -rf /opt/mips_kvs_deplib
tar xvzf mips_kvs_deplib.tar.gz -C /opt/

rm -rf _build
mkdir _build

export COMPILE_TOOL=mips-linux-uclibc-gnu-
export CC=${COMPILE_TOOL}gcc
export CPP=${COMPILE_TOOL}cpp
export CXX=${COMPILE_TOOL}g++
export AS=${COMPILE_TOOL}as
export AR=${COMPILE_TOOL}ar
export LD=${COMPILE_TOOL}ld
export STRIP=${COMPILE_TOOL}strip
export RANLIB=${COMPILE_TOOL}ranlib

export PKG_CONFIG_PATH=/opt/mips_kvs_deplib/lib/pkgconfig
export OPENSSL_ROOT_DIR=/opt/mips_kvs_deplib
export OPENSSL_LIBRARIES=/opt/mips_kvs_deplib/lib

cd _build
cmake ..

make -j4


cp -rvf kvsAacAudioVideoStreamingSample /system/kvs/

