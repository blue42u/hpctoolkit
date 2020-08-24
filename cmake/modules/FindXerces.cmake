# * BeginRiceCopyright *****************************************************
#
# $HeadURL$
# $Id$
#
# --------------------------------------------------------------------------
# Part of HPCToolkit (hpctoolkit.org)
#
# Information about sources of support for research and development of
# HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
# --------------------------------------------------------------------------
#
# Copyright ((c)) 2002-2020, Rice University
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the name of Rice University (RICE) nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# This software is provided by RICE and contributors "as is" and any
# express or implied warranties, including, but not limited to, the
# implied warranties of merchantability and fitness for a particular
# purpose are disclaimed. In no event shall RICE or contributors be
# liable for any direct, indirect, incidental, special, exemplary, or
# consequential damages (including, but not limited to, procurement of
# substitute goods or services; loss of use, data, or profits; or
# business interruption) however caused and on any theory of liability,
# whether in contract, strict liability, or tort (including negligence
# or otherwise) arising in any way out of the use of this software, even
# if advised of the possibility of such damage.
#
# ******************************************************* EndRiceCopyright *

# Xerces ships a pkg-config file, try to use that for hints.
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_Xerces QUIET xerces-c)
  if(PC_Xerces_FOUND)
    set(Xerces_VERSION ${PC_Xerces_VERSION})
  endif()
endif()

find_path(Xerces_INCLUDE_DIR NAMES xercesc/sax2/SAX2XMLReader.hpp
          HINTS ${PC_Xerces_INCLUDE_DIRS})
find_library(Xerces_LIBRARY NAMES xerces-c
             HINTS ${PC_Xerces_LIBRARY_DIRS})
set(_all_library_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})
find_library(Xerces_LIBRARY_STATIC NAMES xerces-c
             HINTS ${PC_Xerces_LIBRARY_DIRS})
set(CMAKE_FIND_LIBRARY_SUFFIXES ${_all_library_suffixes})
unset(_all_library_suffixes)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Xerces
  REQUIRED_VARS Xerces_LIBRARY Xerces_INCLUDE_DIR
  VERSION_VAR Xerces_VERSION)

if(Xerces_FOUND)
  add_library(Xerces::Xerces UNKNOWN IMPORTED)
  set_target_properties(Xerces::Xerces PROPERTIES
                        IMPORTED_LOCATION "${Xerces_LIBRARY}"
                        INTERFACE_INCLUDE_DIRECTORIES "${Xerces_INCLUDE_DIR}")
  if(Xerces_LIBRARY_STATIC)
    add_library(Xerces::Xerces_static UNKNOWN IMPORTED)
    set_target_properties(Xerces::Xerces_static PROPERTIES
                          IMPORTED_LOCATION "${Xerces_LIBRARY_STATIC}"
                          INTERFACE_INCLUDE_DIRECTORIES "${Xerces_INCLUDE_DIR}")
  endif()
endif()
