# Try to find libdw headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(libdw)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  libdw_ROOT         Set this variable to the root installation of
#                     libdw if the module has problems finding the
#                     proper installation path.
#
# Variables defined by this module:
#
#  libdw_FOUND              System has libdw libraries and headers
#  libdw_LIBRARIES          The libdw library
#  libdw_INCLUDE_DIRS       The location of libdw headers
#
# Interface targets defined by this module:
#
#   libdw::libdw
#

message(STATUS "[Findlibdw] === Begin Findlibdw.cmake ===")
message(STATUS "[Findlibdw] Input libdw_ROOT='${libdw_ROOT}'")
message(STATUS "[Findlibdw] Input ENV{libdw_ROOT}='$ENV{libdw_ROOT}'")
message(STATUS "[Findlibdw] Input libdw_INCLUDE_DIR (cached)='${libdw_INCLUDE_DIR}'")
message(STATUS "[Findlibdw] Input libdw_LIBRARY (cached)='${libdw_LIBRARY}'")
message(STATUS "[Findlibdw] CMAKE_PREFIX_PATH='${CMAKE_PREFIX_PATH}'")
message(STATUS "[Findlibdw] CMAKE_LIBRARY_PATH='${CMAKE_LIBRARY_PATH}'")
message(STATUS "[Findlibdw] CMAKE_INCLUDE_PATH='${CMAKE_INCLUDE_PATH}'")
message(STATUS "[Findlibdw] CMAKE_SYSTEM_PREFIX_PATH='${CMAKE_SYSTEM_PREFIX_PATH}'")
message(STATUS "[Findlibdw] CMAKE_FIND_ROOT_PATH='${CMAKE_FIND_ROOT_PATH}'")
message(STATUS "[Findlibdw] ENV{PKG_CONFIG_PATH}='$ENV{PKG_CONFIG_PATH}'")
message(STATUS "[Findlibdw] ENV{PKG_CONFIG_LIBDIR}='$ENV{PKG_CONFIG_LIBDIR}'")
message(STATUS "[Findlibdw] ENV{PKG_CONFIG_SYSTEM_INCLUDE_PATH}='$ENV{PKG_CONFIG_SYSTEM_INCLUDE_PATH}'")

message(STATUS "[Findlibdw] Calling find_package(PkgConfig)")
find_package(PkgConfig)
message(STATUS "[Findlibdw] After find_package(PkgConfig): PkgConfig_FOUND='${PkgConfig_FOUND}' PKG_CONFIG_EXECUTABLE='${PKG_CONFIG_EXECUTABLE}' PKG_CONFIG_VERSION_STRING='${PKG_CONFIG_VERSION_STRING}'")

if(PkgConfig_FOUND)
    message(STATUS "[Findlibdw] Branch: PkgConfig_FOUND is TRUE")
    message(STATUS "[Findlibdw] Clearing ENV{PKG_CONFIG_SYSTEM_INCLUDE_PATH} (was '$ENV{PKG_CONFIG_SYSTEM_INCLUDE_PATH}')")
    set(ENV{PKG_CONFIG_SYSTEM_INCLUDE_PATH} "")
    message(STATUS "[Findlibdw] Calling pkg_check_modules(DW libdw)")
    pkg_check_modules(DW libdw)
    message(STATUS "[Findlibdw] pkg_check_modules results:")
    message(STATUS "[Findlibdw]   DW_FOUND='${DW_FOUND}'")
    message(STATUS "[Findlibdw]   DW_VERSION='${DW_VERSION}'")
    message(STATUS "[Findlibdw]   DW_PREFIX='${DW_PREFIX}'")
    message(STATUS "[Findlibdw]   DW_INCLUDE_DIRS='${DW_INCLUDE_DIRS}'")
    message(STATUS "[Findlibdw]   DW_LIBRARIES='${DW_LIBRARIES}'")
    message(STATUS "[Findlibdw]   DW_LINK_LIBRARIES='${DW_LINK_LIBRARIES}'")
    message(STATUS "[Findlibdw]   DW_LIBRARY_DIRS='${DW_LIBRARY_DIRS}'")
    message(STATUS "[Findlibdw]   DW_LDFLAGS='${DW_LDFLAGS}'")
    message(STATUS "[Findlibdw]   DW_LDFLAGS_OTHER='${DW_LDFLAGS_OTHER}'")
    message(STATUS "[Findlibdw]   DW_CFLAGS='${DW_CFLAGS}'")
    message(STATUS "[Findlibdw]   DW_CFLAGS_OTHER='${DW_CFLAGS_OTHER}'")

    if(DW_FOUND
       AND DW_INCLUDE_DIRS
       AND DW_LINK_LIBRARIES)
        message(STATUS "[Findlibdw] Branch: DW_FOUND AND DW_INCLUDE_DIRS AND DW_LINK_LIBRARIES all TRUE -> using pkg-config result")
        set(libdw_INCLUDE_DIR
            "${DW_INCLUDE_DIRS}"
            CACHE FILEPATH "libdw include directory")
        set(libdw_LIBRARY
            "${DW_LINK_LIBRARIES}"
            CACHE FILEPATH "libdw libraries")
        message(STATUS "[Findlibdw]   Set libdw_INCLUDE_DIR='${libdw_INCLUDE_DIR}'")
        message(STATUS "[Findlibdw]   Set libdw_LIBRARY='${libdw_LIBRARY}'")
        if(DW_PREFIX)
            message(STATUS "[Findlibdw]   Branch: DW_PREFIX is set -> caching libdw_ROOT_DIR")
            set(libdw_ROOT_DIR
                "${DW_PREFIX}"
                CACHE FILEPATH "libdw root directory")
            message(STATUS "[Findlibdw]   Set libdw_ROOT_DIR='${libdw_ROOT_DIR}'")
        else()
            message(STATUS "[Findlibdw]   Branch: DW_PREFIX is empty -> skipping libdw_ROOT_DIR cache")
        endif()

        if(DW_VERSION)
            message(STATUS "[Findlibdw]   Branch: DW_VERSION is set -> caching libdw_VERSION")
            set(libdw_VERSION
                "${DW_VERSION}"
                CACHE FILEPATH "libdw version")
            message(STATUS "[Findlibdw]   Set libdw_VERSION='${libdw_VERSION}'")
        else()
            message(STATUS "[Findlibdw]   Branch: DW_VERSION is empty -> skipping libdw_VERSION cache")
        endif()
    else()
        message(STATUS "[Findlibdw] Branch: pkg-config result incomplete -> NOT caching libdw_* from pkg-config")
        message(STATUS "[Findlibdw]   (DW_FOUND='${DW_FOUND}' DW_INCLUDE_DIRS='${DW_INCLUDE_DIRS}' DW_LINK_LIBRARIES='${DW_LINK_LIBRARIES}')")
    endif()
else()
    message(STATUS "[Findlibdw] Branch: PkgConfig_FOUND is FALSE -> skipping pkg-config probe")
endif()

message(STATUS "[Findlibdw] After pkg-config block: libdw_INCLUDE_DIR='${libdw_INCLUDE_DIR}' libdw_LIBRARY='${libdw_LIBRARY}'")

if(NOT libdw_INCLUDE_DIR OR NOT libdw_LIBRARY)
    message(STATUS "[Findlibdw] Branch: libdw_INCLUDE_DIR or libdw_LIBRARY missing -> running manual find_path/find_library")
    message(STATUS "[Findlibdw]   Calling find_path(libdw_ROOT_DIR NAMES include/elfutils/libdw.h HINTS '${libdw_ROOT}' PATHS '${libdw_ROOT}')")
    find_path(
        libdw_ROOT_DIR
        NAMES include/elfutils/libdw.h
        HINTS ${libdw_ROOT}
        PATHS ${libdw_ROOT})
    message(STATUS "[Findlibdw]   Result libdw_ROOT_DIR='${libdw_ROOT_DIR}'")

    mark_as_advanced(libdw_ROOT_DIR)

    message(STATUS "[Findlibdw]   Calling find_path(libdw_INCLUDE_DIR NAMES elfutils/libdw.h HINTS '${libdw_ROOT}' PATHS '${libdw_ROOT}' PATH_SUFFIXES include)")
    find_path(
        libdw_INCLUDE_DIR
        NAMES elfutils/libdw.h
        HINTS ${libdw_ROOT}
        PATHS ${libdw_ROOT}
        PATH_SUFFIXES include)
    message(STATUS "[Findlibdw]   Result libdw_INCLUDE_DIR='${libdw_INCLUDE_DIR}'")

    message(STATUS "[Findlibdw]   Calling find_library(libdw_LIBRARY NAMES dw HINTS '${libdw_ROOT}' PATHS '${libdw_ROOT}' PATH_SUFFIXES lib lib64)")
    find_library(
        libdw_LIBRARY
        NAMES dw
        HINTS ${libdw_ROOT}
        PATHS ${libdw_ROOT}
        PATH_SUFFIXES lib lib64)
    message(STATUS "[Findlibdw]   Result libdw_LIBRARY='${libdw_LIBRARY}'")
else()
    message(STATUS "[Findlibdw] Branch: libdw_INCLUDE_DIR and libdw_LIBRARY already set -> skipping manual find")
endif()

include(FindPackageHandleStandardArgs)
message(STATUS "[Findlibdw] Final values before find_package_handle_standard_args:")
message(STATUS "[Findlibdw]   libdw_LIBRARY='${libdw_LIBRARY}'")
message(STATUS "[Findlibdw]   libdw_INCLUDE_DIR='${libdw_INCLUDE_DIR}'")
message(STATUS "[Findlibdw]   libdw_ROOT_DIR='${libdw_ROOT_DIR}'")
message(STATUS "[Findlibdw]   libdw_VERSION='${libdw_VERSION}'")
find_package_handle_standard_args(libdw DEFAULT_MSG libdw_LIBRARY libdw_INCLUDE_DIR)
message(STATUS "[Findlibdw] After find_package_handle_standard_args: libdw_FOUND='${libdw_FOUND}'")

if(libdw_FOUND)
    message(STATUS "[Findlibdw] Branch: libdw_FOUND is TRUE -> creating imported target")
    if(NOT TARGET libdw::libdw)
        message(STATUS "[Findlibdw]   Branch: target libdw::libdw does not exist -> creating it")
        add_library(libdw::libdw INTERFACE IMPORTED)

        if(TARGET PkgConfig::DW AND DW_FOUND)
            message(STATUS "[Findlibdw]   Branch: linking libdw::libdw to PkgConfig::DW")
            target_link_libraries(libdw::libdw INTERFACE PkgConfig::DW)
        else()
            message(STATUS "[Findlibdw]   Branch: linking libdw::libdw to libdw_LIBRARY='${libdw_LIBRARY}' with includes '${libdw_INCLUDE_DIR}'")
            target_link_libraries(libdw::libdw INTERFACE ${libdw_LIBRARY})
            target_include_directories(libdw::libdw SYSTEM INTERFACE ${libdw_INCLUDE_DIR})
        endif()
    else()
        message(STATUS "[Findlibdw]   Branch: target libdw::libdw already exists -> reusing")
    endif()
else()
    message(STATUS "[Findlibdw] Branch: libdw_FOUND is FALSE -> imported target not created")
endif()

mark_as_advanced(libdw_INCLUDE_DIR libdw_LIBRARY)
message(STATUS "[Findlibdw] === End Findlibdw.cmake ===")
