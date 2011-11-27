prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
includedir=${prefix}/include
libdir=${exec_prefix}/lib

Name: GKrellMd
Description: Server for GKrellM
Version: @GKRELLM_VERSION_MAJOR@.@GKRELLM_VERSION_MINOR@.@GKRELLM_VERSION_PATCH@
Requires: gtk+-2.0 >= 2.4.0
Cflags: -I${includedir}
Libs: -L${libdir} @GKRELLMD_LINK_ARGS@
