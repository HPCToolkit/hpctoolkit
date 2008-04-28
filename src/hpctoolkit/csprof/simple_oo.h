#ifndef SIMPLE_OO_H
#define SIMPLE_OO_H

// very simple OO for c (mainly to get OO method interface)
//
// no class structure (so no inheiritance or other sophisticated OO techniques)
//  object = (pointer to a) struct with components that are function pointers
//  initializing the object SYSTEM requires setting the method fields to
//  appropriate functions (ideally these are static functions, but common
//  functions might be global)
//
//  methods are C functions that take self (a pointer to the object) as
//  it's first argument.
//
//  Since it is impossible to define macros that define other macros,
//  each .h file that defines a specific object type will need to
//  define METHOD_DEF and METHOD_FN macros like the example below
//  The METHOD_CALL macro, however, is general for all objects that
//  follow this simple protocol.
//
// #define METHOD_DEF(retn,name,args...) retn (*name)(struct _obj_s *self,args)
//
// // abbreviation macro for common case of void methods
// #define VMETHOD_DEF(name,args...) METHOD_DEF(void,name,args)

// #define METHOD_FN(name,...) name(obj1 *self, ##__VA_ARGS__)

//
// objects follow the prototype below:
// 
// typedef struct _obj_s {
//
//  METHOD_DEF(void,m1,void);    // defines method m1(void)
//  METHOD_DEF(void,m2,int d);   // defines method m2(int d)
//
//  int data[10];
// } obj1;

// NOTEME: gcc specific use of ##__VA_ARGS__ (necessary for methods w no aux args)

#define METHOD_CALL(obj,meth,...) (obj)->meth((obj), ##__VA_ARGS__)

#endif // SIMPLE_OO_H
