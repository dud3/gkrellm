
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

set(GKRELLM_INCLUDES
    gkrellm.h
    gkrellm-public-proto.h
    )

# This is a client build (flag needed for sysdep code)
add_definitions(-DGKRELLM_CLIENT)

# TODO: Configurable PACKAGE?
add_definitions(-DPACKAGE="gkrellm")

# For included pixmaps
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../)

include_directories(${GTK2_INCLUDE_DIRS})
include_directories(${GDK_PIXBUF_INCLUDE_DIRS})

# ----------------------------------------------------------------------------
# Platform specific sources and defines
if (WIN32)
    set(GKRELLM_SOURCES ${GKRELLM_SOURCES}
        winops-win32.c win32-plugin.c win32-resource.rc
        )
elseif (APPLE AND (NOT X11_FOUND))
    set(GKRELLM_SOURCES ${GKRELLM_SOURCES}
        winops-gtk-mac.c)
elseif (X11_FOUND)
    set(GKRELLM_SOURCES ${GKRELLM_SOURCES}
        winops-x11.c)
else ()
    message(FATAL_ERROR "No supported windowing system found")
endif ()

# SSL (either OpenSSL or GnuTLS)
if ( (NOT ${OPENSSL_FOUND}) AND (NOT ${GNUTLS_FOUND}) )
     # Use own md5 implementation
     set(GKRELLM_SOURCES ${GKRELLM_SOURCES} md5c.c)
endif ()

add_executable(gkrellm WIN32 ${GKRELLM_SOURCES})
if (WIN32)
    target_link_libraries(gkrellm ws2_32 wtsapi32 pdh iphlpapi)
    # Build static helper-library for plugin development on win32
    add_library(libgkrellm STATIC win32-libgkrellm.c)
    set_target_properties(libgkrellm PROPERTIES OUTPUT_NAME gkrellm)
    install(TARGETS libgkrellm
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )
    set(GKRELLM_LINK_ARG -lgkrellmd) # for gkrellm.pc
endif ()

# Always link to Gtk+ 2.x
target_link_libraries(gkrellm
    ${GTK2_LIBRARIES}
    ${GMODULE2_LIBRARIES}
    ${GTHREAD2_LIBRARIES}
    ${GDK_PIXBUF_LIBRARIES}
    )

if (APPLE)
    target_link_libraries(gkrellm ${IOKIT_LIBRARY})
endif ()

if (OPENSSL_FOUND)
    # Link to OpenSSL if it was found
    target_link_libraries(gkrellm ${OPENSSL_LIBRARIES})
elseif (GNUTLS_FOUND AND GNUTLS_OPENSSL_FOUND AND (GNUTLS_NEW_VERSION OR GCRYPT_FOUND))
    # Link to GnuTLS if it was found
    target_link_libraries(gkrellm ${GNUTLS_LIBRARIES} ${GNUTLS_OPENSSL_LIBRARIES})
    # Link to GCrypt if it is needed
    if (NOT(GNUTLS_NEW_VERSION))
        target_link_libraries(gkrellm ${GCRYPT_LIBRARIES})
    endif ()
endif ()

if (${NTLM_FOUND})
    target_link_libraries(gkrellm ${NTLM_LIBRARIES})
endif ()

if (${SENSORS_FOUND})
    target_link_libraries(gkrellm ${SENSORS_LIBRARIES})
endif ()

if (${X11_FOUND})
    target_link_libraries(gkrellm ${X11_X11_LIB} ${X11_ICE_LIB} ${X11_SM_LIB})
endif ()

if (LIBINTL_FOUND)
    target_link_libraries(gkrellm ${LIBINTL_LIBRARIES})
endif ()

install(TARGETS gkrellm RUNTIME DESTINATION bin)

install(FILES ${GKRELLM_INCLUDES} DESTINATION include/gkrellm2)

configure_file(gkrellm.pc.cmake gkrellm.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/gkrellm.pc DESTINATION lib/pkgconfig)
