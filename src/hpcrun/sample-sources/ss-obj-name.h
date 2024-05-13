// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef SS_OBJ_NAME_H
#define SS_OBJ_NAME_H

#define SSON_cat2(a, b) a ## b
#define SSON_cat3(a, b, c) a ## b ## c

#define SS_OBJ_NAME(ssname) SSON_cat3(_,ssname,_obj)


#define SS_OBJ_CONSTRUCTOR_HELPER2(first, second) SSON_cat2(first, second)
#define SS_OBJ_CONSTRUCTOR_HELPER1(ssname) SS_OBJ_CONSTRUCTOR_HELPER2(hpcrun_constructor_ss_, ssname)
#define SS_OBJ_CONSTRUCTOR(ssname) SS_OBJ_CONSTRUCTOR_HELPER1(SS_OBJ_NAME(ssname))

#endif // SS_OBJ_NAME_H
