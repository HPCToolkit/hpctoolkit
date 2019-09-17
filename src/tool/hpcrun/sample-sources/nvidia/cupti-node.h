#ifndef _HPCTOOLKIT_CUPTI_NODE_H_
#define _HPCTOOLKIT_CUPTI_NODE_H_

#include <cupti_activity.h>
#include <hpcrun/cct2metrics.h>
#include <hpcrun/sample-sources/gpu/gpu-record.h>



void
cupti_activity_entry_set
        (
                entry_activity_t *entry,
                CUpti_Activity *activity,
                cct_node_t *cct_node
        );



#endif
