// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_ROCTRACER_H
#define HPCRUN_FOIL_ROCTRACER_H

#include <roctracer/roctracer_ext.h>
#include <roctracer/roctracer_hip.h>

roctracer_status_t f_roctracer_open_pool_expl(const roctracer_properties_t*,
                                              roctracer_pool_t**);
roctracer_status_t f_roctracer_flush_activity_expl(roctracer_pool_t*);
roctracer_status_t
    f_roctracer_activity_push_external_correlation_id(activity_correlation_id_t);
roctracer_status_t
f_roctracer_activity_pop_external_correlation_id(activity_correlation_id_t*);
roctracer_status_t f_roctracer_enable_domain_callback(activity_domain_t,
                                                      activity_rtapi_callback_t, void*);
roctracer_status_t f_roctracer_enable_domain_activity_expl(activity_domain_t,
                                                           roctracer_pool_t*);
roctracer_status_t f_roctracer_disable_domain_callback(activity_domain_t);
roctracer_status_t f_roctracer_disable_domain_activity(activity_domain_t);
roctracer_status_t f_roctracer_set_properties(activity_domain_t, void*);
const char* f_roctracer_op_string(uint32_t domain, uint32_t op, uint32_t kind);

#endif // HPCRUN_FOIL_ROCTRACER_H
