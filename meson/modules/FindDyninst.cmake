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

include(FindPackageHandleStandardArgs)

# Dyninst consists of a series of components.
set(_all_components common dynC_API dynDwarf dynElf dyninstAPI
    instructionAPI parseAPI pcontrol stackwalk symLite symtabAPI)

# The Dyninst components have internal dependencies. This is effectively a
# table that marks them all down.
set(_common_deps common)
set(_dynElf_deps dynElf)
set(_instructionAPI_deps instructionAPI ${_common_deps})
set(_dynDwarf_deps dynDwarf ${_common_deps} ${_dynElf_deps})
set(_symLite_deps symLite ${_common_deps} ${_dynElf_deps})
set(_symtabAPI_deps symtabAPI ${_common_deps} ${_dynDwarf_deps} ${_dynElf_deps})
set(_parseAPI_deps parseAPI ${_instructionAPI_deps} ${_symtabAPI_deps})
set(_pcontrol_deps pcontrol ${_symtabAPI_deps})
set(_stackwalk_deps stackwalk ${_pcontrol_deps} ${_parseAPI_deps}
    ${_instructionAPI_deps} ${_symtabAPI_deps} ${_dynDwarf_deps} ${_common_deps}
    ${_dynElf_deps})
set(_dyninstAPI_deps dyninstAPI ${_stackwalk_deps} ${_pcontrol_deps}
    ${_patchAPI_deps} ${_parseAPI_deps} ${_symtabAPI_deps}
    ${_instructionAPI_deps})
set(_dynC_API_deps dynC_API ${_dyninstAPI_deps})

# Nab the path to the include directory. It gets stitched into every component.
find_path(Dyninst_INCLUDE_DIR NAMES Symtab.h
          DOC "Location of the Dyninst include directory")

# Search for each of the bits. Let the standard code handle the usual bits.
foreach(C ${_all_components})
  find_library(Dyninst_${C}_LIBRARY NAMES ${C}
               DOC "Location of the Dyninst ${C} component")

  if(Dyninst_${C}_LIBRARY)
    set(Dyninst_${C}_FOUND TRUE)
  else()
    set(Dyninst_${C}_FOUND FALSE)
  endif()

  if(Dyninst_${C}_FOUND AND Dyninst_INCLUDE_DIR)
    set(_link_deps)
    foreach(D ${_${C}_deps})
      if(NOT D STREQUAL C)
        list(APPEND _list_deps Dyninst::${D})
      endif()
    endforeach()
    add_library(Dyninst::${C} UNKNOWN IMPORTED)
    set_target_properties(Dyninst::${C} PROPERTIES
                          IMPORTED_LOCATION "${Dyninst_${C}_LIBRARY}"
                          INTERFACE_INCLUDE_DIRECTORIES "${Dyninst_INCLUDE_DIR}"
                          INTERFACE_LINK_LIBRARIES "${_link_deps}")
    unset(_link_deps)
  endif()
endforeach()

# Finish up with the final status message from the common code
find_package_handle_standard_args(Dyninst
  REQUIRED_VARS Dyninst_dyninstAPI_LIBRARY Dyninst_INCLUDE_DIR
  HANDLE_COMPONENTS)

foreach(C ${_all_components})
  unset(_${C}_deps)
endforeach()
unset(_all_components)
