// $Id$
// -*-C-*-
// * BeginRiceCopyright *****************************************************
/*
  Copyright ((c)) 2002-2007, Rice University 
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  * Neither the name of Rice University (RICE) nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  This software is provided by RICE and contributors "as is" and any
  express or implied warranties, including, but not limited to, the
  implied warranties of merchantability and fitness for a particular
  purpose are disclaimed. In no event shall RICE or contributors be
  liable for any direct, indirect, incidental, special, exemplary, or
  consequential damages (including, but not limited to, procurement of
  substitute goods or services; loss of use, data, or profits; or
  business interruption) however caused and on any theory of liability,
  whether in contract, strict liability, or tort (including negligence
  or otherwise) arising in any way out of the use of this software, even
  if advised of the possibility of such damage.
*/
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//    csprof_csdata.h
//
// Purpose:
//    The basic interface to some structure that stores call stack
//    samples.  This interface doesn't add any overhead and has been
//    useful in switching quickly between different backends.
//    However, it may not be terribly useful anymore.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef CSPROF_CSDATA_H
#define CSPROF_CSDATA_H

//************************* System Include Files ****************************

#include <stdio.h>

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

//***************************************************************************

#include "cct.h"

// public types
typedef csprof_cct_t csprof_csdata_t;

// public function interface
#define csprof_csdata__init          csprof_cct__init
#define csprof_csdata__fini          csprof_cct__fini
#define csprof_csdata__insert_bt     csprof_cct__insert_bt
#define csprof_csdata__write_txt     csprof_cct__write_txt
#define csprof_csdata__write_bin     csprof_cct__write_bin
#define csprof_csdata_insert_backtrace_list	csprof_cct_insert_backtrace_list
#define csprof_csdata_insert_backtrace csprof_cct_insert_backtrace
#endif
