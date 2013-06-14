#ifndef _OMPT_INTERFACE_H_
#define _OMPT_INTERFACE_H_

//------------------------------------------------------------------------------
// function: 
//   ompt_state_is_overhead
// 
// description:
//   returns 1 if the current state represents a form of overhead
//------------------------------------------------------------------------------
extern int ompt_state_is_overhead();

extern int ompt_elide_frames();
extern int ompt_elide_frames();
extern int ompt_outermost_parallel_id();

#endif // _OMPT_INTERFACE_H_
