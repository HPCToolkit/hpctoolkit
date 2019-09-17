//
// Created by user on 17.8.2019..
//

#ifndef HPCTOOLKIT_AMD_H
#define HPCTOOLKIT_AMD_H


#include <hpcrun/cct/cct.h>
#include <hpcrun/sample-sources/gpu/gpu-record.h>

void roctracer_activity_attribute(entry_activity_t *activity, cct_node_t *cct_node);

void roctracer_init();
void roctracer_fini();


#endif //HPCTOOLKIT_AMD_H
