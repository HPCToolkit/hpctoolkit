// -*-Mode: C++;-*- // technically C99
// $Id$

#ifndef unwind_cfg_h
#define unwind_cfg_h

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

#define HPC_UNW_LITE 0
#define HPC_UNW_MSG  1

//***************************************************************************

// HPC_UNW_LITE: When this is set, we assume unwinding must work in a
// very restricted environment such as a SEGV handler.  Performance is
// not critical.  This is a useful starting point for abstracting the
// unwinder into a library.
//
// - We must avoid dynamic allocation which rules out the use of
//   csprof_malloc.  This implies we cannot use 'build_intervals()' to
//   build all the intervals for a routine.  Nor can we use the interval
//   splay tree to store the intervals across invocations of unw_step().
//   Therefore we must use intervals in a special way and pass them
//   around as objects rather than pointers.  This last requirement
//   means that the platform-independent unwind cursor needs to store
//   the whole platform-specific interval information.
//
// - We cannot use libmonitor to test for stack bottom.
//
// - We cannot use the fnbounds server
//
// - We cannot rely on a SEGV handler to correct unwinding mistakes
// 
// - We may wish to avoid the 'pmsg' message printing library


#if (HPC_UNW_LITE)
#  define HPC_IF_UNW_LITE(x)   x
#  define HPC_IFNO_UNW_LITE(x) /* remove */
#else
#  define HPC_IF_UNW_LITE(x)   /* remove */
#  define HPC_IFNO_UNW_LITE(x) x
#endif


//***************************************************************************

#endif // unwind_cfg_h
