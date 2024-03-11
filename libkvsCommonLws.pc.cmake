prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
includedir=${prefix}/include
libdir=${exec_prefix}/@CMAKE_INSTALL_LIBDIR@

Name: KVS-libkvsCommonLws
Description: KVS C Producer common libwebsockets library
Version: @KINESIS_VIDEO_PRODUCER_C_VERSION@
Cflags: -I${includedir}
Libs: -L${libdir} -lkvsCommonLws
