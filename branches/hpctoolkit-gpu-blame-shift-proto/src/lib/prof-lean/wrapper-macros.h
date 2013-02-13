#ifndef _WRAPPER_MACROS_H_
#define _WRAPPER_MACROS_H_
//*****************************************************************************
// File:
//   wrapper-macros.h
//
// Purpose:
//   use C99 variadic macros to count and cast a list of macro arguments 
//
// Modification History
// 
//  2010/12/27 - created Bill Scherer and John Mellor-Crummey
//
//*****************************************************************************


//*****************************************************************************
// VA_DECLARE_ARGSXX: 
//    a set of macros to construct an argument list by pasting together type and 
//    variable pairs 
//*****************************************************************************
#define VA_DECLARE_ARG(type_t, x) type_t x

#define VA_DECLARE_ARGS20(type_t, x, ...) \
	VA_DECLARE_ARG(type_t, x), VA_DECLARE_ARGS18(__VA_ARGS__)

#define VA_DECLARE_ARGS18(type_t, x, ...) \
	VA_DECLARE_ARG(type_t, x), VA_DECLARE_ARGS16(__VA_ARGS__)

#define VA_DECLARE_ARGS16(type_t, x, ...) \
	VA_DECLARE_ARG(type_t, x), VA_DECLARE_ARGS14(__VA_ARGS__)

#define VA_DECLARE_ARGS14(type_t, x, ...) \
	VA_DECLARE_ARG(type_t, x), VA_DECLARE_ARGS12(__VA_ARGS__)

#define VA_DECLARE_ARGS12(type_t, x, ...) \
	VA_DECLARE_ARG(type_t, x), VA_DECLARE_ARGS10(__VA_ARGS__)

#define VA_DECLARE_ARGS10(type_t, x, ...) \
	VA_DECLARE_ARG(type_t, x), VA_DECLARE_ARGS8(__VA_ARGS__)

#define VA_DECLARE_ARGS8(type_t, x, ...) \
	VA_DECLARE_ARG(type_t, x), VA_DECLARE_ARGS6(__VA_ARGS__)

#define VA_DECLARE_ARGS6(type_t, x, ...) \
	VA_DECLARE_ARG(type_t, x), VA_DECLARE_ARGS4(__VA_ARGS__)

#define VA_DECLARE_ARGS4(type_t, x, ...) \
	VA_DECLARE_ARG(type_t, x), VA_DECLARE_ARGS2(__VA_ARGS__)

#define VA_DECLARE_ARGS2(type_t, x, ...) \
	VA_DECLARE_ARG(type_t, x)

#define VA_DECLARE_ARGS1(type_t, ...) type_t

#define VA_DECLARE_ARGS0(...) void


//*****************************************************************************
// VA_CALL_ARGSXX: 
//    a set of macros to cast each of a sequence of arguments in a __VA_ARGS__ 
//    list to type_t
//*****************************************************************************
#define VA_CALL_ARG(type_t, x) x

#define VA_CALL_ARGS20(type_t, x, ...) \
	VA_CALL_ARG(type_t, x), VA_CALL_ARGS18(__VA_ARGS__)

#define VA_CALL_ARGS18(type_t, x, ...) \
	VA_CALL_ARG(type_t, x), VA_CALL_ARGS16(__VA_ARGS__)

#define VA_CALL_ARGS16(type_t, x, ...) \
	VA_CALL_ARG(type_t, x), VA_CALL_ARGS14(__VA_ARGS__)

#define VA_CALL_ARGS14(type_t, x, ...) \
	VA_CALL_ARG(type_t, x), VA_CALL_ARGS12(__VA_ARGS__)

#define VA_CALL_ARGS12(type_t, x, ...) \
	VA_CALL_ARG(type_t, x), VA_CALL_ARGS10(__VA_ARGS__)

#define VA_CALL_ARGS10(type_t, x, ...) \
	VA_CALL_ARG(type_t, x), VA_CALL_ARGS8(__VA_ARGS__)

#define VA_CALL_ARGS8(type_t, x, ...) \
	VA_CALL_ARG(type_t, x), VA_CALL_ARGS6(__VA_ARGS__)

#define VA_CALL_ARGS6(type_t, x, ...) \
	VA_CALL_ARG(type_t, x), VA_CALL_ARGS4(__VA_ARGS__)

#define VA_CALL_ARGS4(type_t, x, ...) \
	VA_CALL_ARG(type_t, x), VA_CALL_ARGS2(__VA_ARGS__)

#define VA_CALL_ARGS2(type_t, x, ...) \
	VA_CALL_ARG(type_t, x) 

#define VA_CALL_ARGS1(type_t,  ...) 

#define VA_CALL_ARGS0(...)

//*****************************************************************************
// VA_COMMA0, VA_COMMA1: 
//    macros that expand to the number of commas indicated by their suffix.
//*****************************************************************************
#define VA_COMMA0
#define VA_COMMA1 ,


//*****************************************************************************
// _VA_CONCAT2: 
//    macro to concatenate a pair of arguments into a name.
//*****************************************************************************
#define _VA_CONCAT2(name,x) name##x


//*****************************************************************************
// _VA_CONCAT3: 
//    macro to concatenate a pair of arguments into a name.
//*****************************************************************************
#define _VA_CONCAT3(x,y,z,...) x##y##z


//*****************************************************************************
// __twentyone_and_0__:
//    helper macro that expands to twentyone placeholder arguments followed
//    by a 0.
//*****************************************************************************
#define __twentyone_and_0__					\
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 0


//*****************************************************************************
// _VA_SELECT22: 
// _VA_SELECT22_INTERNAL: 
//    pair of macros to expand to 22nd argument. VA_SELECT22 macro ensures 
//    that argument list is completely expanded before calling 
//    _VA_SELECT22_INTERNAL
//*****************************************************************************
#define _VA_SELECT22_INTERNAL(_0,_1,_2,_3,_4,_5,_6,_7,			\
			      _8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,...) _21

#define _VA_SELECT22(...) _VA_SELECT22_INTERNAL(__VA_ARGS__)


//*****************************************************************************
// VA_COUNT_ARGS: macro that expands to the number of arguments
// WARNING: will give unpredictable results if more than 16 arguments supplied.
//*****************************************************************************
#define VA_COUNT_ARGS(...)						\
    _VA_SELECT22(_VA_CONCAT3(__twentyone,_and_0__,__VA_ARGS__),		\
		 __VA_ARGS__,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1)


#define X_ARGS(...)						\
    _VA_CONCAT3(__twentyone,_and_0__,__VA_ARGS__)
//*****************************************************************************
// ONE_IF_EMPTY: expand to a 1 if a non-zero number of arguments are supplied.
// WARNING: will give unpredictable results if more than 16 arguments supplied. 
//*****************************************************************************
#define ONE_IF_NONEMPTY(...)						\
    _VA_SELECT22(_VA_CONCAT3(__twentyone,_and_0__,__VA_ARGS__),		\
		 __VA_ARGS__,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1)


//*****************************************************************************
// VA_CONCAT_EVAL:
//    macro to join two parts of a macro name and evaluate the result.
//*****************************************************************************
#define VA_CONCAT2_EVAL(name,x) _VA_CONCAT2(name,x)


//*****************************************************************************
// VA_COMMA_IF_NONEMPTY:
//    macro that expands to a comma if the argument list is not empty.
//*****************************************************************************
#define VA_COMMA_IF_NONEMPTY(...)				\
	VA_CONCAT2_EVAL(VA_COMMA,ONE_IF_NONEMPTY(__VA_ARGS__))


//*****************************************************************************
// VA_DECLARE_ARGS:
//    assembles a function declaration argument list with type variable pairs 
//*****************************************************************************
#define VA_DECLARE_ARGS(...)					\
    VA_CONCAT2_EVAL(VA_DECLARE_ARGS,VA_COUNT_ARGS(__VA_ARGS__))(		\
	__VA_ARGS__)


//*****************************************************************************
// VA_CALL_ARGS:
//    assembles a function argument list containing just a list of variables
//*****************************************************************************
#define VA_CALL_ARGS(...)					\
    VA_CONCAT2_EVAL(VA_CALL_ARGS,VA_COUNT_ARGS(__VA_ARGS__))(		\
	 __VA_ARGS__)

#define VA_FN_CALL(fn, ...)					\
    fn(VA_CONCAT2_EVAL(VA_CALL_ARGS,VA_COUNT_ARGS(__VA_ARGS__))(		\
	__VA_ARGS__))


#define VA_FN_DECLARE(tfn, fn, ...)					\
    tfn fn(VA_CONCAT2_EVAL(VA_DECLARE_ARGS,VA_COUNT_ARGS(__VA_ARGS__))(		\
	__VA_ARGS__))


// Test cases: anytime you change these macros, make sure these tests pass.

#if 0
VA_FN_CALL( Fun)
VA_FN_CALL( Fun, t1, 1)
VA_FN_CALL( Fun, t1, 1, t2, 2)
VA_FN_CALL( Fun, t1, 1, t2, 2, t3, 3)
VA_FN_CALL( Fun, t1, 1, t2, 2, t3, 3, t4, 4)
VA_FN_CALL( Fun, t1, 1, t2, 2, t3, 3, t4, 4, t5, 5)
VA_FN_CALL( Fun, t1, 1, t2, 2, t3, 3, t4, 4, t5, 5, t6, 6)
VA_FN_CALL( Fun, t1, 1, t2, 2, t3, 3, t4, 4, t5, 5, t6, 6, t7, 7)
VA_FN_CALL( Fun, t1, 1, t2, 2, t3, 3, t4, 4, t5, 5, t6, 6, t7, 7, t8, 8)
VA_FN_CALL( Fun, t1, 1, t2, 2, t3, 3, t4, 4, t5, 5, t6, 6, t7, 7, t8, 8 , t9, 9)
VA_FN_CALL( Fun, t1, 1, t2, 2, t3, 3, t4, 4, t5, 5, t6, 6, t7, 7, t8, 8 , t9, 9, t10 *, 10)

VA_FN_DECLARE(ret, Fun)
VA_FN_DECLARE(ret, Fun, void)
VA_FN_DECLARE(ret, Fun, t1, 1)
VA_FN_DECLARE(ret, Fun, t1, 1, t2, 2)
VA_FN_DECLARE(ret, Fun, t1, 1, t2, 2, t3, 3)
VA_FN_DECLARE(ret, Fun, t1, 1, t2, 2, t3, 3, t4, 4)
VA_FN_DECLARE(ret, Fun, t1, 1, t2, 2, t3, 3, t4, 4, t5, 5)
VA_FN_DECLARE(ret, Fun, t1, 1, t2, 2, t3, 3, t4, 4, t5, 5, t6, 6)
VA_FN_DECLARE(ret, Fun, t1, 1, t2, 2, t3, 3, t4, 4, t5, 5, t6, 6, t7, 7)
VA_FN_DECLARE(ret, Fun, t1, 1, t2, 2, t3, 3, t4, 4, t5, 5, t6, 6, t7, 7, t8, 8)
VA_FN_DECLARE(ret, Fun, t1, 1, t2, 2, t3, 3, t4, 4, t5, 5, t6, 6, t7, 7, t8, 8 , t9, 9)
VA_FN_DECLARE(ret, Fun, t1, 1, t2, 2, t3, 3, t4, 4, t5, 5, t6, 6, t7, 7, t8, 8 , t9, 9, t10 *, 10)
#endif

#endif
