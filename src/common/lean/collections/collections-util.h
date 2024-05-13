// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef GENERIC_COLLECTIONS_UTIL_H
#define GENERIC_COLLECTIONS_UTIL_H 1

#define IMPL_CAT2(x,y) x##y

#define CAT2(x, y) IMPL_CAT2(x, y)
#define CAT3(x, y, z) CAT2(x, CAT2(y, z))

#define JOIN2(x, y) CAT3(x, _, y)
#define JOIN3(x, y, z) JOIN2(x, JOIN2(y, z))

#define DECLARE 1
#define DEFINE 2
#define DEFINE_INPLACE 3

#endif //GENERIC_COLLECTIONS_UTIL_H
