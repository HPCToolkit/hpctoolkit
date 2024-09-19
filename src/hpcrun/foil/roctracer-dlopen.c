// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "roctracer-private.h"

const struct hpcrun_foil_appdispatch_roctracer hpcrun_dispatch_roctracer = {
    .roctracer_open_pool_expl = &roctracer_open_pool_expl,
    .roctracer_flush_activity_expl = &roctracer_flush_activity_expl,
    .roctracer_activity_push_external_correlation_id =
        &roctracer_activity_push_external_correlation_id,
    .roctracer_activity_pop_external_correlation_id =
        &roctracer_activity_pop_external_correlation_id,
    .roctracer_enable_domain_callback = &roctracer_enable_domain_callback,
    .roctracer_enable_domain_activity_expl = &roctracer_enable_domain_activity_expl,
    .roctracer_disable_domain_callback = &roctracer_disable_domain_callback,
    .roctracer_disable_domain_activity = &roctracer_disable_domain_activity,
    .roctracer_set_properties = &roctracer_set_properties,
    .roctracer_op_string = &roctracer_op_string,
};
