#ifndef atomic_op_gcc_h
#define atomic_op_gcc_h

//***************************************************************************
//
// File: atomic-op-gcc.h 
//
// Description:
//   implement the small set of atomic operations needed by hpctoolkit
//   using gcc atomics
//
// Author:
//   23 July 2009 - John Mellor-Crummey - created
//
//***************************************************************************

//***************************************************************************
// macros
//***************************************************************************

#define fetch_and_add(addr, val) \
	__sync_fetch_and_add(addr, val) 

#define fetch_and_add_i32(addr, val) \
	fetch_and_add(addr, val) 

#define fetch_and_add_i64(addr, val) \
	fetch_and_add(addr, val) 


#define atomic_add(addr, val) \
	(void) fetch_and_add(addr, val) 

#define atomic_add_i32(addr, val) \
	atomic_add(addr, val) 

#define atomic_add_i64(addr, val) \
	atomic_add(addr, val) 



// -------------------------------------------------------------
// requirement: _sync_lock_test_and_set(addr, newval) must 
// implement an exchange on your architecture. if not, this 
// won't work.
// -------------------------------------------------------------
#define fetch_and_store(addr, newval)  \
	__sync_lock_test_and_set(addr, newval) 

#define fetch_and_store_i32(addr, newval) \
	fetch_and_store(addr, newval) 

#define fetch_and_store_i64(addr, newval) \
	fetch_and_store(addr, newval) 

#define fetch_and_store_ptr(addr, newval) \
	fetch_and_store(addr, newval)


#define compare_and_swap(addr, oldval, newval) \
	__sync_val_compare_and_swap(addr, oldval, newval)

#define compare_and_swap_i32(addr, oldval, newval) \
	compare_and_swap(addr, oldval, newval) 

#define compare_and_swap_i64(addr, oldval, newval) \
	compare_and_swap(addr, oldval, newval) 

#define compare_and_swap_ptr(addr, oldval, newval) \
	compare_and_swap(addr, oldval, newval) 



#endif
