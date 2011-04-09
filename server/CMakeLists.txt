
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

# This is a server build (flag needed for sysdep code)
add_definitions(-DGKRELLM_SERVER)

# TODO: Configurable PACKAGE?
add_definitions(-DPACKAGE_D="gkrellmd")

# ----------------------------------------------------------------------------
# Platform specific sources and defines
if (WIN32)
    set(GKRELLMD_SOURCES ${GKRELLMD_SOURCES}
        win32-plugin.c win32-resource.rc
        )
endif ()

# TODO: libgtop?

add_executable(gkrellmd ${GKRELLMD_SOURCES})
if (WIN32)
    target_link_libraries(gkrellmd ws2_32 wtsapi32 pdh iphlpapi)
    # Build static helper-library for plugin development on win32
    add_library(libgkrellmd STATIC win32-libgkrellmd.c)
    set_target_properties(libgkrellmd PROPERTIES OUTPUT_NAME gkrellmd)
    install(TARGETS libgkrellmd
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )
endif ()

target_link_libraries(gkrellmd
    ${GLIB2_LIBRARIES}
    ${GMODULE2_LIBRARIES}
    ${GTHREAD2_LIBRARIES}
    )

if (APPLE)
    target_link_libraries(gkrellmd ${IOKIT_LIBRARY})
endif ()

if (SENSORS_FOUND)
    target_link_libraries(gkrellmd ${SENSORS_LIBRARIES})
endif ()

install(TARGETS gkrellmd RUNTIME DESTINATION bin)
