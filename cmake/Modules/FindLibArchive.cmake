# Try to find LibArchive
# NOTE: The FindLibArchive.cmake included with CMake has no support for static libraries, so we use our own.
#
# Set LibArchive_USE_STATIC_LIBS=ON to look for static libraries.
#
# Once done this will define:
#  LibArchive_FOUND - Whether LibArchive was found on the system
#  LibArchive_INCLUDE_DIR - The LibArchive include directories
#  LibArchive_VERSION - The version of LibArchive installed on the system
#
# Conventions:
# - Variables only for use within the script are prefixed with "libarchive_"
# - Variables that should be externally visible are prefixed with "LibArchive_"

# Run pkg-config
find_package(PkgConfig)
pkg_check_modules(libarchive_PKGCONF QUIET libarchive)

# Set include directory
find_path(LibArchive_INCLUDE_DIR archive.h
        HINTS ${libarchive_PKGCONF_INCLUDEDIR}
        PATH_SUFFIXES include
        )

# Handle static libraries
if(LibArchive_USE_STATIC_LIBS)
    # Save current value of CMAKE_FIND_LIBRARY_SUFFIXES
    set(libarchive_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})

    # Temporarily change CMAKE_FIND_LIBRARY_SUFFIXES to static library suffix
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
endif()

# Find library
find_library(LibArchive_LIBRARY
        NAMES archive
        HINTS ${libarchive_PKGCONF_LIBDIR}
        PATH_SUFFIXES lib
        )
if (LibArchive_LIBRARY)
    # NOTE: This must be set for find_package_handle_standard_args to work
    set(LibArchive_FOUND ON)
endif()

find_package(ZLIB REQUIRED)

if(LibArchive_USE_STATIC_LIBS)
    # Restore original value of CMAKE_FIND_LIBRARY_SUFFIXES
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${libarchive_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
    unset(libarchive_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES)
endif()

# Find LZ4 Library
if(LibArchive_USE_STATIC_LIBS)
    set(LZ4_USE_STATIC_LIBS ON)
endif()
find_package(LZ4 REQUIRED)

# Find OpenSSL Library
if(LibArchive_USE_STATIC_LIBS)
    set(OPENSSL_USE_STATIC_LIBS ON)
endif()
find_package(OpenSSL REQUIRED)

# Set external dependencies
set(libarchive_EXTERNAL_DEPENDENCIES
        LZ4::LZ4
        OpenSSL::Crypto
        ZLIB::ZLIB
        )

# Set version
set(LibArchive_VERSION ${libarchive_PKGCONF_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibArchive
        REQUIRED_VARS LibArchive_INCLUDE_DIR
        VERSION_VAR LibArchive_VERSION
        )

if(NOT TARGET LibArchive::LibArchive)
    # Add library to build
    if (LibArchive_FOUND)
        if (LibArchive_USE_STATIC_LIBS)
            add_library(LibArchive::LibArchive STATIC IMPORTED)
        else()
            # NOTE: We use UNKNOWN so that if the user doesn't have the SHARED libraries installed, we can still use the STATIC libraries
            add_library(LibArchive::LibArchive UNKNOWN IMPORTED)
        endif()
    endif()

    # Set include directories for library
    if(LibArchive_INCLUDE_DIR)
        set_target_properties(LibArchive::LibArchive
                PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${LibArchive_INCLUDE_DIR}"
                )
    endif()

    # Set location of library
    if(EXISTS "${LibArchive_LIBRARY}")
        set_target_properties(LibArchive::LibArchive
                PROPERTIES
                IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                IMPORTED_LOCATION "${LibArchive_LIBRARY}"
                )

        # Add component's dependencies for linking
        if(libarchive_EXTERNAL_DEPENDENCIES)
            set_target_properties(LibArchive::LibArchive
                    PROPERTIES
                    INTERFACE_LINK_LIBRARIES "${libarchive_EXTERNAL_DEPENDENCIES}"
                    )
        endif()
    endif()
endif()
