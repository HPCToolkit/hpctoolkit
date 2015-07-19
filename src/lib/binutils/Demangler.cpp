// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: $
// $Id: $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2014, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File: Demangler.cpp
//   $HeadURL: $
//
// Purpose: 
//   Implement an API that enables an HPCToolkit user to provide and employ 
//   an arbitrary C++ Standard to demangle symbols.
//
// Description:
//   The API includes an interface to register a C++ Standard Library that
//   will be used to demangle symbols and a demangler interface that will
//   employ the specified library to perform demangling.
//
//***************************************************************************



//******************************************************************************
// global includes
//******************************************************************************

#include <iostream>

#include <stdio.h>
#include <dlfcn.h>
#include <cxxabi.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "Demangler.hpp"



//******************************************************************************
// macros
//******************************************************************************

#define CXX_DEMANGLER_FN_NAME "__cxa_demangle"



//******************************************************************************
// type definitions
//******************************************************************************

typedef char *(*demangler_t)
  (const char *mangled_name, 
   char *output_buffer, 
   size_t *length, 
   int *status);



//******************************************************************************
// local variables
//******************************************************************************

static const char *demangler_library_filename = NULL;

static void       *demangler_library_handle   = NULL;

static demangler_t demangle_fn                = NULL;



//******************************************************************************
// private operations
//******************************************************************************

static void
hpctoolkit_demangler_error(char *error_string)
{
  std::cerr << "WARNING: Unable to open user-specified C++ demangler library '" 
            << demangler_library_filename << "'" << std::endl; 

  std::cerr << "         Dynamic library error: '" << error_string <<  "'" 
            << std::endl; 

  std::cerr << "         Using default demangler instead." << std::endl;
}


static void
hpctoolkit_demangler_init()
{
  if (demangler_library_filename) {
    demangler_library_handle = 
      dlopen(demangler_library_filename, RTLD_LAZY | RTLD_LOCAL);

    if (demangler_library_handle) {
      dlerror(); // clear error condition before calling dlsym

      demangle_fn = (demangler_t) 
        dlsym(demangler_library_handle, CXX_DEMANGLER_FN_NAME);

      if (demangle_fn) return;
    }
    hpctoolkit_demangler_error(dlerror());
  } 

  demangle_fn = abi::__cxa_demangle;
}



//******************************************************************************
// interface operations
//******************************************************************************

void 
hpctoolkit_demangler_library(const char *filename)
{
  demangler_library_filename = filename;
}


char *
hpctoolkit_demangle(const char *mangled_name, 
                    char *output_buffer, 
                    size_t *length, 
                    int *status)
{
  if (demangle_fn == NULL) hpctoolkit_demangler_init();  

  return demangle_fn(mangled_name, output_buffer, length, status);
}
