#ifndef CSPROF_TRAMPOLINE_H
#define CSPROF_TRAMPOLINE_H

/* this should probably not be changed, as there is a severe performance
   penalty for saving the FP regs, even in non-call intensive programs. */

#define TRAMPOLINE_SAVE_ALL_FP_REGISTERS 0

/* floating-point registers $f2-$f9 are defined in the calling standard
   as "conventional saved registers.  if a standard-conforming procedure
   modifies one of these registers, it must save and restore it."  we
   seem to be hitting some floating-point exception errors (25/08/2004),
   so this is an experiment in seeing if we can make those go away by
   judiciously saving registers--saving all of the FP registers is a huge
   hit on performance, so we're trying to avoid that if at all possible. */

#if defined(CSPROF_PERF)
#define TRAMPOLINE_SAVE_SAVED_FP_REGISTERS 0
#else
#define TRAMPOLINE_SAVE_SAVED_FP_REGISTERS 1
#endif

#if TRAMPOLINE_SAVE_ALL_FP_REGISTERS && TRAMPOLINE_SAVE_SAVED_FP_REGISTERS
#error "Cannot define both FP register saves at the same time."
#endif

/* allocate the appropriate amount of stack space */

#define TRAMPOLINE_SAVE_ALL_INT_REGISTERS 1

/* TRAMPOLINE_INT_REGISTER_STACK_SPACE */
#if TRAMPOLINE_SAVE_ALL_INT_REGISTERS
#define TIRSS 240
#else
#define TIRSS 120
#endif

#if TRAMPOLINE_SAVE_ALL_FP_REGISTERS
#define TRAMP_STACK_SPACE (TIRSS + 256)
#elif TRAMPOLINE_SAVE_SAVED_FP_REGISTERS
#define TRAMP_STACK_SPACE (TIRSS + 192)
#else
#define TRAMP_STACK_SPACE TIRSS
#endif

/* define the floating-point save register mask */

#if TRAMPOLINE_SAVE_ALL_FP_REGISTERS
#define TRAMP_FPMASK 0x7fffffff
#elif TRAMPOLINE_SAVE_SAVED_FP_REGISTERS
#define TRAMP_FPMASK 0x7ffffb03
#else
#define TRAMP_FPMASK 0x0
#endif

#endif
