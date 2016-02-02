/*
 * generic_val.h
 *
 * A variable of type void* epresents an abstract notion of a value in a tree
 * or list node.
 *
 *      Author: dxnguyen
 */

#ifndef __GENERIC_VAL__
#define __GENERIC_VAL__

#if 0
// abstract function to compute and return a string representation of
// a generic value.
typedef char* (*val_str)(void* val);
#endif

// abstract function to compute a string representation of a generic value
// and return the result in the str parameter.
// caller to provide the appropriate length for the result string.
typedef void (*val_tostr)(void* val, char str[]);


/*
 * Abstract comparator function to compare two generic values: lhs vs rhs
 * Returns:
 * 0 if lhs matches rhs
 * < 0 if lhs < rhs
 * > 0 if lhs > rhs
 */
typedef int (*val_cmp)(void* lhs, void *rhs);

#endif /* __GENERIC_VAL__ */
