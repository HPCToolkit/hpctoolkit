// SPDX-FileCopyrightText: 2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#include "collections-util.h"

#include <stdlib.h>


/* Check if freelist prefix is defined */
#ifdef FREELIST_PREFIX
  #define FREELIST_MEMBER(NAME) JOIN2(FREELIST_PREFIX, NAME)
#else
  #define FREELIST_MEMBER(NAME) JOIN2(freelist, NAME)
#endif


#if !defined(FREELIST_DECLARE) && !defined(FREELIST_DEFINE)
  #define FREELIST_DEFINE_INPLACE
#endif


#if !defined(FREELIST_ENTRY_DATA_FIELD)
  #define FREELIST_ENTRY_DATA_FIELD
#endif

/* Define freelist type */
#if defined(FREELIST_DECLARE) || defined(FREELIST_DEFINE_INPLACE)

  /* Check if FREELIST_ENTRY_TYPE is defined */
  #if !defined(FREELIST_ENTRY_TYPE)
    #error FREELIST_ENTRY_TYPE is not defined
  #endif

  /* Define FREELIST_TYPE */
  #define FREELIST_TYPE FREELIST_MEMBER(t)
  typedef struct FREELIST_TYPE {
    FREELIST_ENTRY_TYPE *head;
    void *(*alloc_fn)(size_t);
  } FREELIST_TYPE;

  #define FREELIST_INIITALIZER(ALLOC_FN) { .head = NULL, .alloc_fn = ALLOC_FN }
#endif


#if defined(FREELIST_DECLARE)

void
FREELIST_MEMBER(init)
(
 FREELIST_TYPE *freelist,
 void *(*alloc_fn)(size_t)
);


FREELIST_ENTRY_TYPE *
FREELIST_MEMBER(allocate)
(
 FREELIST_TYPE *freelist
);


void
FREELIST_MEMBER(free)
(
 FREELIST_TYPE *freelist,
 FREELIST_ENTRY_TYPE *entry
);

#endif


#if defined(FREELIST_DEFINE) || defined(FREELIST_DEFINE_INPLACE)


#ifdef FREELIST_DEFINE_INPLACE
#define FREELIST_FN_MOD __attribute__((unused)) static
#else
#define FREELIST_FN_MOD
#endif


FREELIST_FN_MOD void
FREELIST_MEMBER(init)
(
 FREELIST_TYPE *freelist,
 void *(*alloc_fn)(size_t)
)
{
  freelist->head = NULL;
  freelist->alloc_fn = alloc_fn;
}


FREELIST_FN_MOD FREELIST_ENTRY_TYPE *
FREELIST_MEMBER(allocate)
(
 FREELIST_TYPE *freelist
)
{
  if (freelist->head == NULL) {
    return freelist->alloc_fn(sizeof(FREELIST_ENTRY_TYPE));
  }

  FREELIST_ENTRY_TYPE *entry = freelist->head;
  freelist->head = (*entry)FREELIST_ENTRY_DATA_FIELD.next;
  return entry;
}


FREELIST_FN_MOD void
FREELIST_MEMBER(free)
(
 FREELIST_TYPE *freelist,
 FREELIST_ENTRY_TYPE *entry
)
{
  (*entry)FREELIST_ENTRY_DATA_FIELD.next = freelist->head;
  freelist->head = entry;
}

#endif


#undef FREELIST_PREFIX
#undef FREELIST_MEMBER
#undef FREELIST_DECLARE
#undef FREELIST_DEFINE
#undef FREELIST_DEFINE_INPLACE
#undef FREELIST_ENTRY_TYPE
#undef FREELIST_TYPE
#undef FREELIST_FN_MOD
