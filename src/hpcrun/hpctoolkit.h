// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef _HPCTOOLKIT_H_
#define _HPCTOOLKIT_H_

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((visibility("default")))
void hpctoolkit_sampling_start(void);
__attribute__((visibility("default")))
void hpctoolkit_sampling_stop(void);
__attribute__((visibility("default")))
int  hpctoolkit_sampling_is_active(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif  // ! _HPCTOOLKIT_H_
