// -*-Mode: C++;-*- // technically C99
// $Id$

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
#  include "pmsg.h"
#else
#  define TMSG(x, ...) /*remove*/
#  define EMSG(x, ...) /*remove*/
#endif



//***************************************************************************

#endif // mips_unwind_cfg_h
