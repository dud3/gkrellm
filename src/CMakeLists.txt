
# Add local cmake modules
set(CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules
    ${CMAKE_MODULE_PATH})

include(CheckFunctionExists)

# We need PkgConfig for finding libraries
find_package(PkgConfig REQUIRED)
find_package(Gettext)
find_package(GnuTLS)
find_package(OpenSSL)

# ----------------------------------------------------------------------------
# Sources and defines for all platforms

set(GKRELLM_SOURCES
    main.c
    alerts.c
    battery.c
    base64.c
    clock.c
    cpu.c
    disk.c
    fs.c
    hostname.c
    inet.c
    mail.c
    mem.c
    net.c
    proc.c
    sensors.c
    uptime.c
    chart.c
    client.c
    config.c
    deprecated.c
    gui.c
    krell.c
    panel.c
    plugins.c
    pixops.c
    sysdeps-unix.c
    utils.c
    ../shared/log.c
    )

# This is the gkrellm client (needed for code shared between client and server)
add_definitions(-DGKRELLM_CLIENT)

# TODO: Configurable PACKAGE?
add_definitions(-DPACKAGE="gkrellm")

# For shared headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../shared/)
# For included pixmaps
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../)
# For included configure.h
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# ----------------------------------------------------------------------------
# Platform specific sources and defines
if (WIN32)
    set(GKRELLM_SOURCES ${GKRELLM_SOURCES}
        winops-win32.c win32-plugin.c win32-resource.rc
        )
    # TODO: win32-resource.rc
    # Some defines to make windows.h behave as wanted (0x0501 is WinXP)
    add_definitions(-D_WIN32_WINNT=0x0501 -DWINVER=0x0501 -DWIN32_LEAN_AND_MEAN)
    # Use a relative path for finding locale files
    add_definitions(-DLOCALEDIR="share/locale")
elseif (UNIX)
    # TODO: USE_X11 Flag for OS X?
    set(GKRELLM_SOURCES ${GKRELLM_SOURCES}
        winops-x11.c)
    add_definitions(-DLOCALEDIR="${CMAKE_INSTALL_PREFIX}/share/locale")
endif ()

# ----------------------------------------------------------------------------
# Library and function checks

# GTK
pkg_check_modules(GTK2 gtk+-2.0>=2.4 REQUIRED)
include_directories(${GTK2_INCLUDE_DIRS})
link_directories(${GTK2_LIBRARY_DIRS})

# GMODULE
pkg_check_modules(GMODULE2 gmodule-2.0 REQUIRED)
include_directories(${GMODULE2_INCLUDE_DIRS})
link_directories(${GMODULE2_LIBRARY_DIRS})

# GTHREAD
pkg_check_modules(GTHREAD2 gthread-2.0 REQUIRED)
include_directories(${GTHREAD2_INCLUDE_DIRS})
link_directories(${GTHREAD2_LIBRARY_DIRS})

# GDK PIXBUF
pkg_check_modules(GDK_PIXBUF gdk-pixbuf-2.0 REQUIRED)
include_directories(${GDK_PIXBUF_INCLUDE_DIRS})
link_directories(${GDK_PIXBUF_LIBRARY_DIRS})

# GETADDRINFO
if (WIN32)
    check_include_file(ws2tcpip.h HAVE_WS2TCPIP_H)
    if (HAVE_WS2TCPIP_H)
        set(HAVE_GETADDRINFO TRUE)
        set(HAVE_GETHOSTBYNAME TRUE)
    endif ()
elseif (UNIX)
    check_function_exists(getaddrinfo HAVE_GETADDRINFO)
    check_function_exists(gethostbyname HAVE_GETHOSTBYNAME)
endif ()

# GETTEXT
if (GETTEXT_FOUND)
    # TODO: Replace with a HAVE_GETTEXT define in configure.h
    add_definitions(-DENABLE_NLS)
endif ()

# SSL (either OpenSSL or GnuTLS)
if (OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    link_directories(${OPENSSL_LIBARY_DIRS})
    set(HAVE_SSL 1)
elseif (GNUTLS_FOUND)
    include_directories(${GNUTLS_INCLUDE_DIR})
    link_directories(${GNUTLS_LIBRARY_DIRS})
    set(HAVE_SSL 1)
    set(HAVE_GNUTLS 1)
else ()
    # Use own md5 implementation
    set(GKRELLM_SOURCES ${GKRELLM_SOURCES} md5c.c)
endif ()

# NTLM
pkg_check_modules(NTLM libntlm)
if (NTLM_FOUND)
    include_directories(${NTLM_INCLUDE_DIRS})
    link_directories(${NTLM_LIBRARY_DIRS})
    set(HAVE_NTLM 1)
endif (NTLM_FOUND)

if (UNIX)
    # SENSORS
    find_package(Sensors)
    if (SENSORS_FOUND)
        set(HAVE_LIBSENSORS TRUE)
        include_directories(${SENSORS_INCLUDE_DIR})
        link_directories(${SENSORS_LIBRARY_DIRS})
    endif ()

    # X11
    # TODO: Drop X11-dependency when compiling on OS X Gtk+ wih quartz backend
    find_package(X11 REQUIRED)
    include_directories(${X11_INCLUDE_DIR})
    link_directories(${X11_LIBRARY_DIRS})
endif ()


configure_file(configure.h.cmake configure.h)

if (WIN32)
    add_executable(gkrellm WIN32 ${GKRELLM_SOURCES})
    target_link_libraries(gkrellm ws2_32 wtsapi32 pdh iphlpapi)
    # Build static helper-library for plugin development on win32
    add_library(gkrellm_ STATIC win32-libgkrellm.c)
else ()
    add_executable(gkrellm ${GKRELLM_SOURCES})
endif ()

# Always link to Gtk+ 2.x
target_link_libraries(gkrellm
    ${GTK2_LIBRARIES}
    ${GMODULE2_LIBRARIES}
    ${GTHREAD_LIBRARIES}
    ${GDK_PIXBUF_LIBRARIES}
    )

if (OPENSSL_FOUND)
    # Link to OpenSSL if it was found
    target_link_libraries(gkrellm ${OPENSSL_LIBRARIES})
elseif (GNUTLS_FOUND)
    # Link to GnuTLS if it was found
    target_link_libraries(gkrellm ${GNUTLS_LIBRARIES} gnutls-openssl gcrypt)
endif (OPENSSL_FOUND)

if (NTLM_FOUND)
    target_link_libraries(gkrellm ${NTLM_LIBRARIES})
endif (NTLM_FOUND)

if (SENSORS_FOUND)
    target_link_libraries(gkrellm ${SENSORS_LIBRARIES})
endif (SENSORS_FOUND)

if (X11_FOUND)
    target_link_libraries(gkrellm ${X11_X11_LIB} ${X11_ICE_LIB} ${X11_SM_LIB})
endif (X11_FOUND)

