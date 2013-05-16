/*
 * IBM Confidential
 * OCO Source Materials
 * IBM XL C/C++ for Blue Gene/Q (5799-AG1)
 * IBM XL Fortran for Blue Gene/Q 
 * 5799-AG1, 5799-AH1
 * Copyright IBM Corp. 2006, 2012.
 * The source code for this program is not published 
 * or otherwise divested of its trade secrets, irrespective 
 * of what has been deposited with the U.S. Copyright Office.
 */

///////////////////////////////////////////////////////////////////////////////
// IBM Light OpenMP C++ Implementation
// Content: USER public interface to omp
// Missing: 
//
// Original author: Alexandre Eichenberger alexe@us.ibm.com
// File: omp.h 
// Log:
//   v1, october 2012, separated from source/omp.h

#ifndef _OMP_H_
#define _OMP_H_ 1

#define _OPENMP
////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#ifdef __cplusplus
  #define _OMP_EXTERN extern "C"
  #define _OMP_INLINE static inline
#else
  #define _OMP_EXTERN extern
  #define _OMP_INLINE static __inline
#endif

////////////////////////////////////////////////////////////////////////////////
// OpenMP types

typedef uint32_t omp_lock_t;      // arbitrary type of the right length
typedef uint64_t omp_nest_lock_t; // arbitrary type of the right length

typedef enum omp_sched_t {
  omp_sched_static         = 1, /* chunkSize >0 */
  omp_sched_dynamic        = 2, /* chunkSize >0 */
  omp_sched_guided         = 3, /* chunkSize >0 */
  omp_sched_auto           = 4, /* no chunkSize */
} omp_sched_t;

typedef enum omp_proc_bind_t {
  omp_proc_bind_false = 0,
  omp_proc_bind_true = 1,
  omp_proc_bind_master = 2,
  omp_proc_bind_close = 3,
  omp_proc_bind_spread = 4
} omp_proc_bind_t;

////////////////////////////////////////////////////////////////////////////////
// Has OpenMP
////////////////////////////////////////////////////////////////////////////////

#if defined(_OPENMP)

_OMP_EXTERN double omp_get_wtick(void); 
_OMP_EXTERN double omp_get_wtime(void); 

_OMP_EXTERN void omp_set_num_threads(int num); 
_OMP_EXTERN int  omp_get_num_threads(void);  
_OMP_EXTERN int  omp_get_max_threads(void); 
_OMP_EXTERN int  omp_get_thread_limit(void); 
_OMP_EXTERN int  omp_get_thread_num(void); 
_OMP_EXTERN int  omp_get_num_procs(void); 
_OMP_EXTERN int  omp_in_parallel(void); 
_OMP_EXTERN int  omp_in_final(void); 

_OMP_EXTERN void omp_set_dynamic(int flag); 
_OMP_EXTERN int  omp_get_dynamic(void);  

_OMP_EXTERN void omp_set_nested(int flag);
_OMP_EXTERN int  omp_get_nested(void);

_OMP_EXTERN void omp_set_max_active_levels(int level);
_OMP_EXTERN int  omp_get_max_active_levels(void);

_OMP_EXTERN int  omp_get_level(void); 
_OMP_EXTERN int  omp_get_active_level(void); 
_OMP_EXTERN int  omp_get_ancestor_thread_num(int level); 
_OMP_EXTERN int  omp_get_team_size(int level); 

_OMP_EXTERN void omp_init_lock(omp_lock_t *lock);
_OMP_EXTERN void omp_init_nest_lock(omp_nest_lock_t *lock);

_OMP_EXTERN void omp_destroy_lock(omp_lock_t *lock);
_OMP_EXTERN void omp_destroy_nest_lock(omp_nest_lock_t *lock);

_OMP_EXTERN void omp_set_lock(omp_lock_t *lock);
_OMP_EXTERN void omp_set_nest_lock(omp_nest_lock_t *lock);

_OMP_EXTERN void omp_unset_lock(omp_lock_t *lock);
_OMP_EXTERN void omp_unset_nest_lock(omp_nest_lock_t *lock);

_OMP_EXTERN int  omp_test_lock(omp_lock_t *lock);
_OMP_EXTERN int  omp_test_nest_lock(omp_nest_lock_t *lock);

_OMP_EXTERN void omp_get_schedule(omp_sched_t * kind, int * modifier); 
_OMP_EXTERN void omp_set_schedule(omp_sched_t kind, int modifier); 

_OMP_EXTERN omp_proc_bind_t omp_get_proc_bind(void); 

// non standard
_OMP_EXTERN void omp_set_proc_bind(omp_proc_bind_t kind); 
_OMP_EXTERN void omp_get_places(char *str, int len);
_OMP_EXTERN int omp_get_thread_place();

#else 

////////////////////////////////////////////////////////////////////////////////
// Does not have OpenMP
////////////////////////////////////////////////////////////////////////////////

_OMP_INLINE double omp_get_wtick(void)                        { return 0.0; }
_OMP_INLINE double omp_get_wtime(void)                        { return 0.0; }

_OMP_INLINE void omp_set_schedule(omp_sched_t kind, int modifier) { }
_OMP_INLINE int omp_get_schedule(omp_sched_t * kind, int * modifier) 
  { return omp_sched_static; }
_OMP_INLINE omp_proc_bind_t omp_get_proc_bind(void) 
  { return omp_proc_bind_false; }

_OMP_INLINE void omp_set_num_threads(int num)                 { }
_OMP_INLINE int omp_get_num_threads(void)                     { return 1; }
_OMP_INLINE int omp_get_max_threads(void)                     { return 1; }
_OMP_INLINE int omp_get_thread_limit(void)                    {  return 1; }
_OMP_INLINE int omp_get_thread_num(void)                      { return 0; }
_OMP_INLINE int omp_get_num_procs(void)                       { return 1; }
_OMP_INLINE int omp_in_parallel(void)                         { return 0; }
_OMP_INLINE int omp_in_final(void)                            { return 0; }
_OMP_INLINE void omp_set_dynamic(int flag)                    { }
_OMP_INLINE int omp_get_dynamic(void)                         { return 0; }
_OMP_INLINE void omp_set_nested(int flag)                     { }
_OMP_INLINE int omp_get_nested(void)                          { return 0; }
_OMP_INLINE void omp_set_max_active_levels(int level)         { }
_OMP_INLINE int omp_get_max_active_levels(int level)          { return 1; }
_OMP_INLINE int omp_get_level(void)                           { return 0; }
_OMP_INLINE int omp_get_active_level(void)                    { return 0; }
_OMP_INLINE int omp_get_ancestor_thread_num(int level)        { return 0; }
_OMP_INLINE int omp_get_team_size(int level)                  { return 1; }
							      
_OMP_INLINE void omp_init_lock(omp_lock_t  *lock)             { }
_OMP_INLINE void omp_destroy_lock(omp_lock_t *lock)           { }
_OMP_INLINE void omp_set_lock(omp_lock_t *lock)               { }
_OMP_INLINE void omp_unset_lock(omp_lock_t *lock)             { }
_OMP_INLINE int omp_test_lock(omp_lock_t *lock)               { return 1; }
							      
_OMP_INLINE void omp_init_nest_lock(omp_nest_lock_t *lock)    { *lock = 0; }
_OMP_INLINE void omp_destroy_nest_lock(omp_nest_lock_t *lock) { }
_OMP_INLINE void omp_set_nest_lock(omp_nest_lock_t *lock)     { (*lock)++; }
_OMP_INLINE void omp_unset_nest_lock(omp_nest_lock_t *lock)   { (*lock)--; }
_OMP_INLINE int omp_test_nest_lock(omp_nest_lock_t *lock) 
  { (*lock)++; return *lock; }

// non standard
_OMP_INLINE void omp_set_proc_bind(omp_proc_bind_t kind)       { }
_OMP_INLINE void omp_get_places(char *str, int len)            { }
_OMP_INLINE int omp_get_thread_place()                         { return -1; }

#endif

////////////////////////////////////////////////////////////////////////////////
// TOOLS type and enums
////////////////////////////////////////////////////////////////////////////////

typedef union ompt_data_t {
    long long value;    /* data under tool control                 */
    void *ptr;          /* pointer under tool control              */
} ompt_data_t;

typedef long long ompt_parallel_id_t;
#define ompt_parallel_id_none ((ompt_parallel_id_t) 0)

typedef long long ompt_wait_id_t;
#define ompt_wait_id_none ((ompt_wait_id_t) 0)

typedef struct ompt_frame_t {
    void *exit_runtime_frame;       /* next frame is user code     */
    void *reenter_runtime_frame;    /* previous frame is user code */
} ompt_frame_t;

typedef enum ompt_state_t {
  /* work states (0..15) */
  ompt_state_work_serial      = 0x00, /* working outside parallel   */
  ompt_state_work_parallel    = 0x01, /* working within parallel    */
  ompt_state_work_reduction   = 0x02, /* performing a reduction     */

  /* idle (16..31) */
  ompt_state_idle             = 0x10, /* waiting for work           */

  /* overhead states (32..63) */
  ompt_state_overhead         = 0x20, /* overhead excluding wait 
                                       * states                     */

  /* wait states non-mutex (64..79) */
  ompt_state_wait_barrier     = 0x40, /* waiting at a barrier       */
  ompt_state_wait_taskwait    = 0x41, /* waiting at a taskwait      */
  ompt_state_wait_taskgroup   = 0x42, /* waiting at a taskgroup     */
	
  /* wait states mutex (80..95) */	      	    
  ompt_state_wait_lock        = 0x50, /* waiting for lock           */
  ompt_state_wait_nest_lock   = 0x51, /* waiting for nest lock      */
  ompt_state_wait_critical    = 0x52, /* waiting for critical       */
  ompt_state_wait_atomic      = 0x53, /* waiting for atomic         */
  ompt_state_wait_ordered     = 0x54, /* waiting for ordered        */

  /* misc (96..127) */
  ompt_state_undefined        = 0x60, /* undefined thread state     */
} ompt_state_t;


typedef enum ompt_event_t {  
  /*--- Mandatory Events ---*/          
  ompt_event_parallel_create        = 1,  /* parallel create        */
  ompt_event_parallel_exit          = 2,  /* parallel exit          */
  				          			    		      
  ompt_event_task_create            = 3,  /* task create            */
  ompt_event_task_exit              = 4,  /* task destroy           */
  				          			    
  ompt_event_thread_create          = 5,  /* thread create          */
  ompt_event_thread_exit            = 6,  /* thread exit            */
				          			    
  ompt_event_control                = 7,  /* support control calls  */
  								    	      
  /*--- Optional Events (blame shifting) ---*/  		    
  ompt_event_idle_begin		    = 8,  /* begin idle state       */ 
  ompt_event_idle_end		    = 9,  /* end idle state         */ 
				    				    
  ompt_event_wait_barrier_begin     = 10, /* begin wait at barrier  */
  ompt_event_wait_barrier_end       = 11, /* end wait at barrier    */
  ompt_event_wait_taskwait_begin    = 12, /* begin wait at taskwait */
  ompt_event_wait_taskwait_end      = 13, /* end wait at taskwait   */
  ompt_event_wait_taskgroup_begin   = 14, /* begin wait at taskgroup*/
  ompt_event_wait_taskgroup_end     = 15, /* end wait at taskgroup  */
				    				    
  ompt_event_release_lock           = 16, /* lock release           */
  ompt_event_release_nest_lock_last = 17, /* last nest lock release */
  ompt_event_release_critical       = 18, /* critical release       */
  ompt_event_release_atomic         = 19, /* atomic release         */
  ompt_event_release_ordered        = 20, /* ordered release        */
								    
  /*--- Optional Events (asynchronous events) --- */        	    
  ompt_event_task_switch_async      = 21, /* non-sched task switch  */
								    
								    
  /*--- Optional Events (synchronous events) --- */        	    
  ompt_event_implicit_task_create   = 22, /* implicit task create   */
  ompt_event_implicit_task_exit     = 23, /* implicit task destroy  */
				    
  ompt_event_task_switch_sched      = 24, /* scheduled task switch  */
				    				    
  ompt_event_loop_begin             = 25, /* task at loop begin     */
  ompt_event_loop_end               = 26, /* task at loop end       */
  ompt_event_section_begin          = 27, /* task at section begin  */
  ompt_event_section_end            = 28, /* task at section end    */
  ompt_event_single_in_block_begin  = 29, /* task at single begin   */
  ompt_event_single_in_block_end    = 30, /* task at single end     */
  ompt_event_single_others_begin    = 31, /* task at single begin   */
  ompt_event_single_others_end      = 32, /* task at single end     */
			              	 			    
  ompt_event_master_begin           = 33, /* task at master begin   */
  ompt_event_master_end             = 34, /* task at master end     */
  ompt_event_barrier_begin          = 35, /* task at barrier begin  */
  ompt_event_barrier_end            = 36, /* task at barrier end    */
  ompt_event_taskwait_begin         = 37, /* task at taskwait begin */
  ompt_event_taskwait_end           = 38, /* task at task wait end  */
  ompt_event_taskgroup_begin        = 39, /* task at taskgroup begin*/
  ompt_event_taskgroup_end          = 40, /* task at taskgroup end  */

  ompt_event_release_nest_lock_prev = 41, /* prev nest lock release */
								    
  ompt_event_wait_lock              = 42, /* lock wait              */
  ompt_event_wait_nest_lock         = 43, /* nest lock wait         */
  ompt_event_wait_critical          = 44, /* critical wait          */
  ompt_event_wait_atomic            = 45, /* atomic wait            */
  ompt_event_wait_ordered           = 46, /* ordered wait           */
		 	            				    	    
  ompt_event_acquired_lock          = 47, /* lock acquired          */
  ompt_event_acquired_nest_lock_first=48, /* 1st nest lock acquired */
  ompt_event_acquired_nest_lock_next= 49, /* next nest lock acquired*/
  ompt_event_acquired_critical      = 50, /* critical acquired      */
  ompt_event_acquired_atomic        = 51, /* atomic acquired        */
  ompt_event_acquired_ordered       = 52, /* ordered acquired       */
			            				    	    
  ompt_event_init_lock              = 53, /* lock init              */
  ompt_event_init_nest_lock         = 54, /* nest lock init         */
  ompt_event_destroy_lock           = 55, /* lock destruction       */
  ompt_event_destroy_nest_lock      = 56, /* nest lock destruction  */
			            	 			    	    
  ompt_event_flush                  = 57, /* after executing flush  */ 

} ompt_event_t;

typedef void (*ompt_thread_callback_t) (			   
  ompt_data_t *thread_data           /* tool data for thread       */
  );

typedef void (*ompt_parallel_callback_t) (			   
  ompt_data_t *task_data,           /* tool data for a task        */
  ompt_parallel_id_t parallel_id    /* id of parallel region       */
  );
							   
typedef void (*ompt_new_parallel_callback_t) (
  ompt_data_t  *parent_task_data,   /* tool data for parent task   */
  ompt_frame_t *parent_task_frame,  /* frame data of parent task   */
  ompt_parallel_id_t parallel_id    /* id of parallel region       */
  );								   

typedef void (*ompt_task_callback_t) (			   
  ompt_data_t *task_data            /* tool data for task          */
  );

typedef void (*ompt_task_sched_callback_t) (			   
  ompt_data_t *suspended_task_data, /* tool data for suspended task */ 
  ompt_data_t *resumed_task_data    /* tool data for resumed task   */
  );								   

typedef void (*ompt_new_task_callback_t) (
  ompt_data_t  *parent_task_data,   /* tool data for parent task    */
  ompt_frame_t *parent_task_frame,  /* frame data for parent task   */
  ompt_data_t  *new_task_data       /* tool data for created task   */
  );		
	
typedef void (*ompt_wait_callback_t) (			   
  ompt_wait_id_t wait_id            /* wait id                      */
  );
						   
typedef void (*ompt_control_callback_t) (			   
  long long command,                /* command of control call      */
  long long modifier                /* modifier of control call     */
  );

typedef void (*ompt_callback_t)(void);

////////////////////////////////////////////////////////////////////////////////
// TOOLS with OpenMP
////////////////////////////////////////////////////////////////////////////////

#ifdef _OPENMP

_OMP_EXTERN ompt_state_t ompt_get_state(ompt_wait_id_t *wait_id);
_OMP_EXTERN ompt_data_t *ompt_get_thread_data();
_OMP_EXTERN ompt_parallel_id_t ompt_get_parallel_id(int ancestor_level);
_OMP_EXTERN void *ompt_get_parallel_function(int ancestor_level);
_OMP_EXTERN ompt_data_t *ompt_get_task_data(int ancestor_level);
_OMP_EXTERN ompt_frame_t *ompt_get_task_frame(int ancestor_level);
_OMP_EXTERN void *ompt_get_task_function(int ancestor_level);
_OMP_EXTERN int ompt_register_tool(ompt_callback_t initializer_callback);
_OMP_EXTERN int ompt_set_callback(
  ompt_event_t event, 
  ompt_callback_t callback);
_OMP_EXTERN int ompt_enable_lightweight_tool();
_OMP_EXTERN void ompt_control(long long command, long long modifier);
_OMP_EXTERN char *ompt_get_version();

#else 

////////////////////////////////////////////////////////////////////////////////
// TOOLS without OpenMP
////////////////////////////////////////////////////////////////////////////////

_OMP_INLINE ompt_state_t ompt_get_state(ompt_wait_id_t *wait_id)
{ 
  return ompt_state_undefined; 
}

_OMP_INLINE ompt_data_t *ompt_get_thread_data() 
{
  return 0; 
}

_OMP_INLINE ompt_parallel_id_t ompt_get_parallel_id(int ancestor_level)
{
  return 0; 
}

_OMP_INLINE void *ompt_get_parallel_function(int ancestor_level)
{
  return 0; 
}

_OMP_INLINE ompt_data_t *ompt_get_task_data(int ancestor_level)
{
  return 0; 
}

_OMP_INLINE ompt_frame_t *ompt_get_task_frame(int ancestor_level)
{
  return 0; 
}

_OMP_INLINE void *ompt_get_task_function(int ancestor_level)
{
  return 0; 
}

_OMP_INLINE int ompt_register_tool(ompt_callback_t initializer_callback)
{
  return 0; 
} 

_OMP_INLINE int ompt_set_callback(
  ompt_event_t event, 
  ompt_callback_t callback)
{
  return 0; 
} 

_OMP_INLINE int ompt_enable_lightweight_tool() 
{
  return 0; 
} 

_OMP_INLINE void ompt_control(long long command, long long modifier) 
{
}

_OMP_INLINE char *ompt_get_version() 
{ 
  return "--openmp=false"; 
}
#endif 

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////

#undef _OMP_EXTERN 
#undef _OMP_INLINE 

#endif
