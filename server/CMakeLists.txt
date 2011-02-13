
set(GKRELLMD_SOURCES
    main.c
    monitor.c
    mail.c
    plugins.c
    glib.c
    utils.c
    sysdeps-unix.c
    ../shared/log.c
    )

# This is the gkrellm client (needed for code shared between client and server)
add_definitions(-DGKRELLM_SERVER)

# TODO: Configurable PACKAGE?
add_definitions(-DPACKAGE_D="gkrellmd")

# For included configure.h
#include_directories(${CMAKE_CURRENT_BINARY_DIR})

# ----------------------------------------------------------------------------
# Platform specific sources and defines
if (WIN32)
    set(GKRELLMD_SOURCES ${GKRELLMD_SOURCES}
        win32-plugin.c win32-resource.rc
        )
    # Some defines to make windows.h behave as wanted (0x0501 is WinXP)
    add_definitions(-D_WIN32_WINNT=0x0501 -DWINVER=0x0501 -DWIN32_LEAN_AND_MEAN)
    # Use a relative path for finding locale files
    add_definitions(-DLOCALEDIR="share/locale")
elseif (UNIX)
    add_definitions(-DLOCALEDIR="${CMAKE_INSTALL_PREFIX}/share/locale")
endif ()

# TODO: libgtop?

add_executable(gkrellmd ${GKRELLMD_SOURCES})
if (WIN32)
    target_link_libraries(gkrellmd ws2_32 wtsapi32 pdh iphlpapi)
    # Build static helper-library for plugin development on win32
    add_library(gkrellmd_ STATIC win32-libgkrellmd.c)
endif ()

target_link_libraries(gkrellmd
    ${GLIB2_LIBRARIES}
    ${GMODULE2_LIBRARIES}
    ${GTHREAD2_LIBRARIES}
    )

if (SENSORS_FOUND)
    target_link_libraries(gkrellmd ${SENSORS_LIBRARIES})
endif (SENSORS_FOUND)

