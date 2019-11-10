#ifndef gpu_api_h
#define gpu_api_h

//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/hpcrun-placeholders.h>
#include <gpu/gpu-activity.h>


//******************************************************************************
// interface operations
//******************************************************************************


void
gpu_correlation_callback
(
        uint64_t id,
        placeholder_t state,
        gpu_activity_details_t *entry_data,
	ip_normalized_t *kernel_ip // NULL if not kernel submit
);

#endif
