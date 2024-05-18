// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef support_NaN_h
#define support_NaN_h

//*************************** User Include Files ****************************

#if !defined(__cplusplus)
#include <stdbool.h>
#endif

//****************************************************************************

extern const double c_FP_NAN_d;

#if defined(__cplusplus)
extern "C" {
#endif

bool c_isnan_d(double x);

bool c_isinf_d(double x);

#if defined(__cplusplus)
}
#endif


#endif /* support_NaN_h */
