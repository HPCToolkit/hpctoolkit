#ifndef PPCAT_PPSTR_H
#define PPCAT_PPSTR_H

// Add several Levels of indirection for cpp token pasting (##), 
// and stringize (#) operators.
// These macros are to facilitate other macro definitions that use these features.
// (The indirection is necessary due to quirkyness of cpp expansion algorithm).
//
// If another level of indirection is needed, then add another line
// to the auxiliary functions, and change the main definition to
// use the new level.
//
// Example:
//   Add line
//     #define __CAT3_(a, b) __CAT2_(a, b)
//
//  Then change definition of PPCAT to
//     #define PPCAT(a, b) __CAT3_(a, b)

#define __CAT0_(a, b) a ## b
#define __CAT1_(a, b) __CAT0_(a, b)
#define __CAT2_(a, b) __CAT1_(a, b)

#define PPCAT(a, b) __CAT2_(a, b)

#define __STR0_(a) #a
#define __STR1_(a) __STR0_(a)
#define __STR2_(a) __STR1_(a)

#define PPSTR(a) __STR2_(a)

#endif // PPCAT_PPSTR_H
