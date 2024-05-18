// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef MEMCHK_H
#define MEMCHK_H

#include <stdbool.h>
#include <stddef.h>
static inline bool
memchk(const char *buf,char v,size_t len)
{
  for(int i=0;i < len;i++){
    if (*(buf++) != v){
      return false;
    }
  }
  return true;
}

#endif // MEMCHK_H
