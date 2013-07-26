/* first */
ompt_state(ompt_state_first, 0x71)           /* initial enumeration state */

/* work states (0..15) */
ompt_state(ompt_state_work_serial, 0x00)     /* working outside parallel */
ompt_state(ompt_state_work_parallel, 0x01)   /* working within parallel */
ompt_state(ompt_state_work_reduction, 0x02)  /* performing a reduction */

/* idle (16..31) */
ompt_state(ompt_state_idle, 0x10)            /* waiting for work */

/* overhead states (32..63) */
ompt_state(ompt_state_overhead, 0x20)        /* overhead excluding wait states */

/* barrier wait states (64..79) */
ompt_state(ompt_state_wait_barrier, 0x40)    /* waiting at a barrier */
ompt_state(ompt_state_wait_barrier_implicit, 0x41)    /* implicit barrier */
ompt_state(ompt_state_wait_barrier_explicit, 0x42)    /* explicit barrier */

/* task wait states (80..95) */
ompt_state(ompt_state_wait_taskwait, 0x50)   /* waiting at a taskwait */
ompt_state(ompt_state_wait_taskgroup, 0x51)  /* waiting at a taskgroup */

/* mutex wait states (96..111) */  
ompt_state(ompt_state_wait_lock, 0x60)       /* waiting for lock */
ompt_state(ompt_state_wait_nest_lock, 0x61)  /* waiting for nest lock */
ompt_state(ompt_state_wait_critical, 0x62)   /* waiting for critical */
ompt_state(ompt_state_wait_atomic, 0x63)     /* waiting for atomic */
ompt_state(ompt_state_wait_ordered, 0x64)    /* waiting for ordered */
ompt_state(ompt_state_wait_single, 0x65)     /* waiting for single */

/* misc (112..127) */
ompt_state(ompt_state_undefined, 0x70)       /* undefined thread state */

#undef ompt_state
