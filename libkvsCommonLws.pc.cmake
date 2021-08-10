prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
includedir=${prefix}/include
libdir=${exec_prefix}/@CMAKE_INSTALL_LIBDIR@

Name: KVS-libkvsCommonLws
Description: KVS C Producer common libwebsockets library
Version: 0.0.0
Cflags: -I${includedir}
Libs: -L${libdir} -lkvsCommonLws
