prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
includedir=${prefix}/include
libdir=${exec_prefix}/lib

Name: KVS-libcproducer
Description: KVS C Producer library
Version: @KINESIS_VIDEO_PRODUCER_C_VERSION@
Cflags: -I${includedir}
Libs: -L${libdir} -lcproducer
