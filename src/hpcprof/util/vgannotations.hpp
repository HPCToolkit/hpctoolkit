// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_UTIL_VGANNOTATIONS_H
#define HPCTOOLKIT_PROFILE_UTIL_VGANNOTATIONS_H

#include "../../vendor/valgrind/helgrind.h"
#include "../../vendor/valgrind/drd.h"

#define _GLIBCXX_SYNCHRONIZATION_HAPPENS_BEFORE(addr) ANNOTATE_HAPPENS_BEFORE(addr)
#define _GLIBCXX_SYNCHRONIZATION_HAPPENS_AFTER(addr)  ANNOTATE_HAPPENS_AFTER(addr)

#endif  // HPCTOOLKIT_PROFILE_UTIL_VGANNOTATIONS_H
