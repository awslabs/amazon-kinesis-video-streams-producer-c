prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
includedir=${prefix}/include
libdir=${exec_prefix}/@CMAKE_INSTALL_LIBDIR@

Name: KVS-libkvsCommonCurl
Description: KVS C Producer common curl library
Version: @KINESIS_VIDEO_PRODUCER_C_VERSION@
Cflags: -I${includedir}
Libs: -L${libdir} -lkvsCommonCurl
