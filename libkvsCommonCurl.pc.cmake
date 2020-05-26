prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
includedir=${prefix}/include
libdir=${exec_prefix}/lib

Name: KVS-libkvsCommonCurl
Description: KVS C Producer common curl library
Version: 0.0.0
Cflags: -I${includedir}
Libs: -L${libdir} -lkvsCommonCurl
