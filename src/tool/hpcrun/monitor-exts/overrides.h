#ifndef OVERRIDES_H
#define OVERRIDES_H

// Need macro-ized token-pasting and stringizing operations

#include <utilities/ppcat+ppstr.h>

//
// All overriden functions need a few pieces of syntactic functionality:
//   1) How to syntactically refer to the real function
//        (??_FN)
//   2) How to declare the typedef of the real function
//        (??_TYPEDEF)
//   3) How to declare the syntactic object for the real function
//      (should use 1&2 as components)
//        (??_DCL)
//   4) How to do the lazy initialization of the real function
//      (Some functions need no initialization, others need some kind of lookup)
//        (??_INIT)
//
// The exact syntax, however, may be different for each overridden function.
// In the typical case, the overriden functions fall into a few classes, rather than
// all functions having unique syntax for the components.
//
// NOTE: an additional wrinkle is that static and dynamic linking may require distinct
//       syntax as well.
//
// The client code refers to the syntactic component via the "REAL" prefix
// Ex: REAL_FN(n) expands to the appropriate syntax for the real function,
// no matter what the override class of "n" is.
//
// The macro set below covers 2 different strategies/classes for overrides. The technique
// for selecting the appropriate strategy, however, is extensible. The covered classes
// are:
//
//   1) ALT     : There is a canonically-named alternative function
//                to duplicate the functionality of the overriden function.
//
//                Example: "pthread_mutex_lock" has "__pthred_mutex_lock" as
//                          an alternate entry point
//
//   2) DL      : The real function must be looked up (likely using a dlsym variant)
//                in the dynamic case. (Static case behaves like ALT, but has a
//                different canonical name).
//
// For a given class, there is a syntactic token name prefix (example: ALT and DL)
// The *specific* expansion for each of the functionalities above are embodied in
// a #define macro with the prefix.
// Ex: ALT_FN is syntax for an "ALT"-style real function name.
//     DL_INIT is syntax for lazy initialization of a "DL"-style lazy initialization
//
// The client code defines special macros that specify the style of a given overridden
// function. Suppose "foo" and "bar" are the overridden functions; "foo" is ALT, and "bar"
// is "DL". Then the client code would put these macros in:
//
//   #define foo_REAL ALT
//   #define bar_REAL DL
//
//
// Below is an extremely simple client. The expansion can be see by invoking
//    prompt>gcc -std=gnu99 -E -o example.E example.c
//
// #include <stdio.h>
// #include <string.h>
//
// #include "overrides.h"
//
// #define foo_REAL ALT
// #define bar_REAL DL
//
// REAL_TYPEDEF(int, foo)(int a, int b);
// REAL_TYPEDEF(int, bar)(int a, int b);
//
// REAL_DCL(foo);
// REAL_DCL(bar);
//
// int
// OVERRIDE_NM(foo)(int x, int y)
// {
//   REAL_INIT(foo);
//   REAL_INIT(bar);
//
//   if (x == 5) {
//     return REAL_FN(foo)(x, x+y);
//   }
//   return REAL_FN(bar)(y, x);
// }

//
// The macro source code
//
#define ALT_FN(n)   PPCAT(__, n)
#define ALT_DCL(n) extern REAL_TYPE(n) ALT_FN(n)
#define ALT_TYPEDEF(rt, n) typedef rt REAL_TYPE(n)
#define ALT_INIT(n)

#ifdef HPCRUN_STATIC_LINK
#define DL_FN(n) PPCAT(__real_, n)
#define DL_DCL(n) extern REAL_TYPE(n) DL_FN(n)
#define DL_TYPEDEF(rt, n) typedef rt REAL_TYPE(n)
#define DL_INIT(n)
#define DLV_INIT(n)
#define OVERRIDE_NM(n) PPCAT(__wrap_, n)
#else
#define DL_FN(n) PPCAT(real_, n)
#define DL_DCL(n) static REAL_TYPE(n) DL_FN(n)
#define DL_TYPEDEF(rt, n) typedef rt (*REAL_TYPE(n))
#define DL_INIT(n) if (! DL_FN(n) ){DL_FN(n) = override_lookup(PPSTR(n));}
#define DLV_INIT(n) if (! DL_FN(n) ){DL_FN(n) = override_lookupv(PPSTR(n));}
#define OVERRIDE_NM(n) n
#endif // STATIC_LINK

//
// DLV class is same as DL, except for dynamic case (lookup is different)
//  (see #else case above)
//

#define DLV_FN(n) DL_FN(n)
#define DLV_DCL(n) DL_DCL(n)
#define DLV_TYPEDEF(rt, n) DL_TYPEDEF(rt, n)

#define REAL_TYPE(n) PPCAT(n, _t)

#define REAL_FN(n) PPCAT(PPCAT(n, _REAL),_FN)(n)
#define REAL_TYPEDEF(rt, n) PPCAT(PPCAT(n, _REAL), _TYPEDEF)(rt, n)
#define REAL_DCL(n) PPCAT(PPCAT(n, _REAL), _DCL)(n)
#define REAL_INIT(n) PPCAT(PPCAT(n, _REAL), _INIT)(n)

extern void* override_lookup(char* fname);
extern void* override_lookupv(char* fname);

#endif // OVERRIDES_H
