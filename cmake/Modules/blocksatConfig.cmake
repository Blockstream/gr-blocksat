INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_BLOCKSAT blocksat)

FIND_PATH(
    BLOCKSAT_INCLUDE_DIRS
    NAMES blocksat/api.h
    HINTS $ENV{BLOCKSAT_DIR}/include
        ${PC_BLOCKSAT_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    BLOCKSAT_LIBRARIES
    NAMES gnuradio-blocksat
    HINTS $ENV{BLOCKSAT_DIR}/lib
        ${PC_BLOCKSAT_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(BLOCKSAT DEFAULT_MSG BLOCKSAT_LIBRARIES BLOCKSAT_INCLUDE_DIRS)
MARK_AS_ADVANCED(BLOCKSAT_LIBRARIES BLOCKSAT_INCLUDE_DIRS)

