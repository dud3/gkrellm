cmake_minimum_required(VERSION 2.8)
project(gkrellm C)

# Add local cmake modules
set(CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
    ${CMAKE_MODULE_PATH})

include(CheckFunctionExists)
include(CheckIncludeFile)

# Need pkg-config for finding libraries
find_package(PkgConfig REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/shared/)

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
find_package(Gettext)
if (GETTEXT_FOUND)
    # TODO: add to configure.h
    add_definitions(-DENABLE_NLS)
endif ()

# GLIB2
pkg_check_modules(GLIB2 glib-2.0 REQUIRED)
include_directories(${GLIB2_INCLUDE_DIRS})
link_directories(${GLIB2_LIBRARY_DIRS})

# GMODULE
pkg_check_modules(GMODULE2 gmodule-2.0 REQUIRED)
include_directories(${GMODULE2_INCLUDE_DIRS})
link_directories(${GMODULE2_LIBRARY_DIRS})

# GTHREAD
pkg_check_modules(GTHREAD2 gthread-2.0 REQUIRED)
include_directories(${GTHREAD2_INCLUDE_DIRS})
link_directories(${GTHREAD2_LIBRARY_DIRS})

# GTK (optional, needed for gkrellm client)
pkg_check_modules(GTK2 gtk+-2.0>=2.4)

# Gdk Pixbuf (optional, needed for gkrellm client)
pkg_check_modules(GDK_PIXBUF gdk-pixbuf-2.0)

# IOKit
if (APPLE)
    find_package(IOKit REQUIRED)
endif ()

# NTLM
pkg_check_modules(NTLM libntlm)
if (NTLM_FOUND)
    include_directories(${NTLM_INCLUDE_DIRS})
    link_directories(${NTLM_LIBRARY_DIRS})
    set(HAVE_NTLM TRUE) # for configure.h
endif ()

# SENSORS
if (UNIX)
    find_package(Sensors)
    if (SENSORS_FOUND)
        include_directories(${SENSORS_INCLUDE_DIR})
        link_directories(${SENSORS_LIBRARY_DIRS})
        set(HAVE_LIBSENSORS TRUE) # for configure.h
    endif ()
endif ()

# SSL (either OpenSSL or GnuTLS)
find_package(GnuTLS)
find_package(OpenSSL)
if (OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    link_directories(${OPENSSL_LIBARY_DIRS})
    set(HAVE_SSL TRUE) # for configure.h
elseif (GNUTLS_FOUND)
    include_directories(${GNUTLS_INCLUDE_DIR})
    link_directories(${GNUTLS_LIBRARY_DIRS})
    set(HAVE_SSL TRUE) # for configure.h
    set(HAVE_GNUTLS TRUE) # for configure.h
endif ()

# Windres compiler
if (WIN32)
    set(CMAKE_RC_COMPILER_INIT windres)
    enable_language(RC)
    set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>")
endif ()

if (WIN32)
    # win32 specific flag needed GLib/GThread/Gtk+
    set(CMAKE_C_FLAGS "-mms-bitfields ${CMAKE_C_FLAGS}")
endif ()

# X11
if (UNIX)
    # TODO: Drop X11-dependency when compiling on Gtk+ with Quartz backend
    find_package(X11)
    if (${X11_FOUND})
        include_directories(${X11_INCLUDE_DIR})
        link_directories(${X11_LIBRARY_DIRS})
    endif()
endif ()

# Build configure.h and add location to include-path
configure_file(configure.h.cmake configure.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Options for building either gkrellm, gkrellmd or both
option(GKRELLM_BUILD_CLIENT "Build GKrellM (Client)" TRUE)
option(GKRELLM_BUILD_SERVER "Build GKrellM Daemon" TRUE)

if (${GKRELLM_BUILD_CLIENT})
    if (NOT ${GTK2_FOUND})
        message(ERROR "Cannot build GKrellM (Client), Gtk+ development files missing")
    endif()
    if (NOT ${GDK_PIXBUF_FOUND})
        message(ERROR "Cannot build GKrellM (Client), Gdk-Pixbuf development files missing")
    endif()
    if (NOT ${X11_FOUND})
        message(ERROR "Cannot build GKrellM (Client), X11 development files missing")
    endif()
    add_subdirectory(src)
endif ()

# Always build the server, all deps are marked as REQUIRED
if (${GKRELLM_BUILD_SERVER})
    add_subdirectory(server)
endif ()

