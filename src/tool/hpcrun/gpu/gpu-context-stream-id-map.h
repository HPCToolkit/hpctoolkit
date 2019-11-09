#ifndef gpu_context_stream_id_map_h
#define gpu_context_stream_id_map_h

//******************************************************************************
// global includes
//******************************************************************************

#include <pthread.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/splay-macros.h>
#include <lib/prof-lean/stdatomic.h>

#include <monitor.h>

#include <tool/hpcrun/threadmgr.h>
#include <tool/hpcrun/cct/cct.h>

#include "trace.h"


//******************************************************************************
// type declaration
//******************************************************************************

typedef struct gpu_context_stream_id_map_entry_t gpu_context_stream_id_map_entry_t;


//******************************************************************************
// interface operations
//******************************************************************************


gpu_context_stream_id_map_entry_t *
gpu_context_stream_map_lookup
(
 uint32_t context_id,
 uint32_t stream_id
);


void
gpu_context_stream_map_insert
(
 uint32_t context_id,
 uint32_t stream_id
);


void
gpu_context_stream_map_delete
(
 uint32_t context_id,
 uint32_t stream_id
);


void
gpu_context_stream_map_signal_all
(
 void
);


#endif
