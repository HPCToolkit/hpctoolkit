// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef HPCRUN_MESSAGES_ERRORS_H
#define HPCRUN_MESSAGES_ERRORS_H

#ifndef __cplusplus
#include <stdnoreturn.h>
#endif

/// Abort the process. Does not log any messages.
#ifdef __cplusplus
[[noreturn]]
#else
noreturn
#endif
void hpcrun_terminate();

#endif // HPCRUN_MESSAGES_ERRORS_H
