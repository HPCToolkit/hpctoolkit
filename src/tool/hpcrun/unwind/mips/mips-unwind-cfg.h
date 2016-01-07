// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
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
// HPCToolkit's MIPS Unwinder
// 
// Nathan Tallent
// Rice University
//
// When part of HPCToolkit, this code assumes HPCToolkit's license;
// see www.hpctoolkit.org.
//
//***************************************************************************

#ifndef mips_unwind_cfg_h
#define mips_unwind_cfg_h

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "unwind-cfg.h"

//*************************** Forward Declarations **************************

//***************************************************************************

#if (!HPC_UNW_LITE)

// libmonitor includes
#include <monitor.h>

// hpcrun includes
#  include <memory/mem.h>

#  include "splay-interval.h"
#  include "splay.h"
#  include "fnbounds_interface.h"
#  include "ui_tree.h"

#include <lib/prof-lean/atomic-op.h>

#else

// hpcrun includes
#  include "dylib.h"

#endif


#if (HPC_UNW_MSG)
#  include <messages/messages.h>
#else
#  define TMSG(x, ...) /*remove*/
#  define EMSG(x, ...) /*remove*/
#endif



//***************************************************************************

#endif // mips_unwind_cfg_h
