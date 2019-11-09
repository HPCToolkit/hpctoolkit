#ifndef _HPCTOOLKIT_GPU_TRACE_CHANNEL_H_
#define _HPCTOOLKIT_GPU_TRACE_CHANNEL_H_

#include <lib/prof-lean/bi_unordered_channel.h>

#include "gpu-node.h"

#include "gpu-channel.h"

declare_typed_channel(trace, uint64_t start, uint64_t end, cct_node_t *cct_node);

#endif
