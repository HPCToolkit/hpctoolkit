//
// Created by aleksa on 8/16/19.
//

#ifndef HPCTOOLKIT_PLACEHOLDERS_H
#define HPCTOOLKIT_PLACEHOLDERS_H

#include <hpcrun/utilities/ip-normalized.h>

typedef struct {
    void           *pc;
    ip_normalized_t pc_norm;
} placeholder_t;

load_module_t *
pc_to_lm
(
    void *pc
);



void
init_placeholder
(
    placeholder_t *p,
    void *pc
);


#endif //HPCTOOLKIT_PLACEHOLDERS_H
