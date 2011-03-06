# - Try to find the libntlm library
# Once done this will define
#  NTLM_FOUND - system has libntlm
#  NTLM_INCLUDE_DIR - the libntlm include directory
#  NTLM_LIBRARIES - The libraries needed to use libntlm
#  NTLM_DEFINITIONS - Compiler switches required for using libntlm

FIND_PACKAGE(PkgConfig)
PKG_CHECK_MODULES(PC_NTLM libntlm QUIET)

FIND_PATH(NTLM_INCLUDE_DIR ntlm.h
   HINTS
   ${PC_NTLM_INCLUDEDIR}
   ${PC_NTLM_INCLUDE_DIRS}
   )

FIND_LIBRARY(NTLM_LIBRARY NAMES ntlm libntlm
   HINTS
   ${PC_NTLM_LIBDIR}
   ${PC_NTLM_LIBRARY_DIRS}
   )

MARK_AS_ADVANCED(NTLM_INCLUDE_DIR NTLM_LIBRARY)

# handle the QUIETLY and REQUIRED arguments and set NTLM_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(NTLM DEFAULT_MSG NTLM_LIBRARY NTLM_INCLUDE_DIR)

IF(NTLM_FOUND)
    SET(NTLM_LIBRARIES ${NTLM_LIBRARY})
    SET(NTLM_INCLUDE_DIRS ${NTLM_INCLUDE_DIR})
    SET(NTLM_DEFINITIONS ${PC_NTLM_CFLAGS_OTHER})
ENDIF()
