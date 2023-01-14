// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2021-2023 Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

#include "logical/common.h"

#include "cct_backtrace_finalize.h"
#include "files.h"
#include "hpcrun-malloc.h"
#include "messages/messages.h"
#include "thread_data.h"
#include "env.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void cleanup_metadata_store(logical_metadata_store_t*);

static logical_metadata_store_t* metadata = NULL;

// ---------------------------------------
// Initialization & finalization
// ---------------------------------------

void hpcrun_logical_init() {
  metadata = NULL;

#ifdef ENABLE_LOGICAL_PYTHON
  if(hpcrun_get_env_bool("HPCRUN_LOGICAL_PYTHON"))
    hpcrun_logical_python_init();
#endif
}

void hpcrun_logical_fini() {
  for(logical_metadata_store_t* m = metadata; m != NULL; m = m->next)
    cleanup_metadata_store(m);
}

// ---------------------------------------
// Logical stack mutation functions
// ---------------------------------------

#define REGIONS_PER_SEGMENT \
  (sizeof( ((struct logical_region_segment_t*)0)->regions ) \
   / sizeof( ((struct logical_region_segment_t*)0)->regions[0] ))
#define FRAMES_PER_SEGMENT \
  (sizeof( ((struct logical_frame_segment_t*)0)->frames ) \
   / sizeof( ((struct logical_frame_segment_t*)0)->frames[0] ))

void hpcrun_logical_stack_init(logical_region_stack_t* s) {
  s->depth = 0;
  s->head = NULL;
  s->spare = NULL;
  s->subspare = NULL;
}

logical_region_t* hpcrun_logical_stack_top(const logical_region_stack_t* s) {
  if(s->depth == 0) return NULL;
  return &s->head->regions[(s->depth-1) % REGIONS_PER_SEGMENT];
}

logical_frame_t* hpcrun_logical_substack_top(const logical_region_t* r) {
  if(r->subdepth == 0) return NULL;
  return &r->subhead->frames[(r->subdepth-1) % FRAMES_PER_SEGMENT];
}

logical_region_t* hpcrun_logical_stack_push(logical_region_stack_t* s,
                                            const logical_region_t* r) {
  // Figure out where the next element goes, allocate some space if we must
  if(s->depth % REGIONS_PER_SEGMENT == 0) {
    if(s->spare != NULL) {
      // Pop from spare for the next segment
      struct logical_region_segment_t* newhead = s->spare;
      s->spare = newhead->prev;
      newhead->prev = s->head;
      s->head = newhead;
    } else {
      // Allocate a new entry for the next segment
      struct logical_region_segment_t* newhead = hpcrun_malloc(sizeof *newhead);
      newhead->prev = s->head;
      s->head = newhead;
    }
  }
  logical_region_t* next = &s->head->regions[s->depth % REGIONS_PER_SEGMENT];

  // Copy over the region data, and return the new top
  memcpy(next, r, sizeof *next);
  s->depth++;
  next->subdepth = 0;
  next->subhead = NULL;
  TMSG(LOGICAL_CTX, "Pushed region [%d] %p, beforeenter = %p", s->depth, next,
       next->beforeenter.sp);
  return next;
}

logical_frame_t* hpcrun_logical_substack_push(logical_region_stack_t* s,
                                              logical_region_t* r, const logical_frame_t* f) {
  // Figure out where the next element goes, allocate some space if we must
  if(r->subdepth % FRAMES_PER_SEGMENT == 0) {
    if(s->subspare != NULL) {
      // Pop from spare for the next segment
      struct logical_frame_segment_t* newhead = s->subspare;
      s->subspare = newhead->prev;
      newhead->prev = r->subhead;
      r->subhead = newhead;
    } else {
      // Allocate a new entry for the next segment
      struct logical_frame_segment_t* newhead = hpcrun_malloc(sizeof *newhead);
      newhead->prev = r->subhead;
      r->subhead = newhead;
    }
  }
  logical_frame_t* next = &r->subhead->frames[r->subdepth % FRAMES_PER_SEGMENT];

  // Copy over the frame data, and return the new top
  memcpy(next, f, sizeof *next);
  r->subdepth++;
  TMSG(LOGICAL_CTX, "Pushed frame [%d] [%d] %p", s->depth, r->subdepth, next);
  return next;
}

size_t hpcrun_logical_stack_settop(logical_region_stack_t* s, size_t n) {
  if(n >= s->depth) return n - s->depth;
  // Pop off segments until we've got the right number
  size_t n_segs = n / REGIONS_PER_SEGMENT;
  if(n % REGIONS_PER_SEGMENT > 0) n_segs++; // Partially-filled segment
  size_t s_segs = s->depth / REGIONS_PER_SEGMENT;
  if(s->depth % REGIONS_PER_SEGMENT > 0) s_segs++;  // Partially-filled segment
  for(; s_segs > n_segs; s_segs--) {
    struct logical_region_segment_t* newhead = s->head->prev;
    s->head->prev = s->spare;
    s->spare = s->head;
    s->head = newhead;
  }
  // We're within the right segment, so we can just set the depth now.
  s->depth = n;
  TMSG(LOGICAL_CTX, "Settop to [%d]", s->depth);
  return 0;
}

size_t hpcrun_logical_substack_settop(logical_region_stack_t* s, logical_region_t* r, size_t n) {
  if(n >= r->subdepth) return n - r->subdepth;
  // Pop off segments until we've got the right number
  size_t n_segs = n / FRAMES_PER_SEGMENT;
  if(n % FRAMES_PER_SEGMENT > 0) n_segs++; // Partially-filled segment
  size_t r_segs = r->subdepth / FRAMES_PER_SEGMENT;
  if(r->subdepth % FRAMES_PER_SEGMENT > 0) r_segs++;  // Partially-filled segment
  for(; r_segs > n_segs; r_segs--) {
    struct logical_frame_segment_t* newhead = r->subhead->prev;
    r->subhead->prev = s->subspare;
    s->subspare = r->subhead;
    r->subhead = newhead;
  }
  // We're within the right segment, so we can just set the depth now.
  r->subdepth = n;
  TMSG(LOGICAL_CTX, "Settop to [%d] [%d]", s->depth, r->subdepth);
  return 0;
}

// ---------------------------------------
// Backtrace modification engine
// ---------------------------------------

static void logicalize_bt(backtrace_info_t*, int);
static bool logicalize_bt_registered = false;
static cct_backtrace_finalize_entry_t logicalize_bt_entry = {
  .fn = logicalize_bt, .next = NULL,
};
void hpcrun_logical_register() {
  if(!logicalize_bt_registered) {
    cct_backtrace_finalize_register(&logicalize_bt_entry);
    logicalize_bt_registered = true;
  }
}

// NOTE: Only used for producing nicer debug log output.
static const char* name_for(uint16_t lm_id) {
  const load_module_t* lm = hpcrun_loadmap_findById(lm_id);
  return lm == NULL ? "<not found>" : lm->name;
}

static void logicalize_bt(backtrace_info_t* bt, int isSync) {
  thread_data_t* td = hpcrun_get_thread_data();
  if(td->logical_regs.depth == 0) return;  // No need for our services

  TMSG(LOGICAL_UNWIND, "========= Logicalizing backtrace =========");

  frame_t* bt_cur = bt->begin;

  struct logical_region_segment_t* seg;
  size_t off = td->logical_regs.depth % REGIONS_PER_SEGMENT;
  bool first = true;
  for(seg = td->logical_regs.head; seg != NULL; seg = seg->prev) {
    for(; off > 0; off--) {
      logical_region_t* cur = &seg->regions[off-1];

      // Progress the cursor until the first frame_t within this logical region
      frame_t* bt_top = bt_cur;
      if(cur->exit_len > 0) {
        while(1) {
          bool hit = false;
          for(unsigned int i = 0; i < cur->exit_len; ++i) {
            if(bt_cur->cursor.sp == cur->exit[i]) {
              hit = true;
              break;
            }
          }
          if(hit)
            break;

          TMSG(LOGICAL_UNWIND, " sp = %p ip = %s +%p", bt_cur->cursor.sp,
                name_for(bt_cur->ip_norm.lm_id), bt_cur->ip_norm.lm_ip);
          if(bt_cur == bt->last) goto earlyexit;
          bt_cur++;
        }

        // Possibly adjust the cursor back if the logical region requests
        if(cur->afterexit != NULL) {
          frame_t* new_exit = (frame_t*)cur->afterexit(cur, bt_cur, bt_top);
          assert(bt_top <= new_exit && new_exit <= bt_cur && "afterexit gave invalid frame!");
          TMSG(LOGICAL_UNWIND, "== Exit from logical range @ sp = %p (%d after exit @ %p) ==",
               new_exit->cursor.sp, bt_cur-new_exit, cur->exit);
          bt_cur = new_exit;
        } else
          TMSG(LOGICAL_UNWIND, "== Exit from logical range @ sp = %p ==", cur->exit);
      } else {
        assert(first && "Only the topmost logical region can have exit = NULL!");
        TMSG(LOGICAL_UNWIND, "== Within logical range ==");
      }
      first = false;
      frame_t* logical_start = bt_cur;

      // Scan through until we've seen everything including the enter
      while(bt_cur->cursor.sp != cur->beforeenter.sp) {
        TMSG(LOGICAL_UNWIND, " sp = %p ip = %s +%p", bt_cur->cursor.sp,
             name_for(bt_cur->ip_norm.lm_id), bt_cur->ip_norm.lm_ip);
        if(bt_cur == bt->last) goto earlyexit;
        bt_cur++;
      }
      frame_t* logical_end = bt_cur;

      TMSG(LOGICAL_UNWIND, "== Logically the above is replaced by the following ==");
      assert(cur->expected > 0 && "Logical regions should always have at least 1 logical frame!");

      // If needed, move later physical frames down to make room for the logical ones
      if(logical_start + cur->expected > logical_end) {
        size_t newused = bt->last - bt->begin + (logical_start + cur->expected - logical_end);
        ptrdiff_t bt_begin_diff = bt->begin - td->btbuf_beg;
        ptrdiff_t bt_last_diff = bt->last - td->btbuf_beg;
        ptrdiff_t bt_cur_diff = bt_cur - td->btbuf_beg;
        ptrdiff_t logical_start_diff = logical_start - td->btbuf_beg;
        ptrdiff_t logical_end_diff = logical_end - td->btbuf_beg;
        while(td->btbuf_end - td->btbuf_beg < newused) hpcrun_expand_btbuf();
        logical_end = td->btbuf_beg + logical_end_diff;
        logical_start = td->btbuf_beg + logical_start_diff;
        bt_cur = td->btbuf_beg + bt_cur_diff;
        bt->last = td->btbuf_beg + bt_last_diff;
        bt->begin = td->btbuf_beg + bt_begin_diff;

        memmove(logical_start + cur->expected, logical_end, (bt->last - logical_end + 1)*sizeof(frame_t));
        bt->last = logical_start + cur->expected + (bt->last - logical_end);
        logical_end = logical_start + cur->expected;
      }

      // Overwrite the physical frames with logical ones
      void* store = NULL;
      size_t index = 0;
      struct logical_frame_segment_t* subseg = cur->subhead;
      // 1-based index of the current logical frame, or 0 if ran out of frames.
      size_t subnum = cur->subdepth == 0 ? 0 : ((cur->subdepth-1) % FRAMES_PER_SEGMENT) + 1;
      assert((subnum == 0 || subseg != NULL) && "Frames without backing frame-segment!");
      while(cur->generator(cur, &store, index, subnum == 0 ? NULL : &subseg->frames[subnum-1], logical_start + index)) {
        TMSG(LOGICAL_UNWIND, "(logical) ip = %d +%p", (logical_start+index)->ip_norm.lm_id, (logical_start+index)->ip_norm.lm_ip);
        assert(index < cur->expected && "Expected number of logical frames is too low!");
        index++;
        if(subnum > 0) {
          subnum--;
          if(subnum == 0) {
            subseg = subseg->prev;
            subnum = subseg == NULL ? 0 : FRAMES_PER_SEGMENT;
          }
        }
      }
      TMSG(LOGICAL_UNWIND, "(logical) ip = %d +%p",
           (logical_start+index)->ip_norm.lm_id, (logical_start+index)->ip_norm.lm_ip);
      TMSG(LOGICAL_UNWIND, "== Entry to logical range from sp = %p (%d frames of %d expected) ==",
           cur->beforeenter.sp, index+1, cur->expected);
      IF_ENABLED(LOGICAL_UNWIND)
        if(index+1 < cur->expected)
          TMSG(LOGICAL_UNWIND, "== WARNING less frames than expected generated above! == ");

      // Shift the remaining physical frames down to fill in any gaps
      bt_cur = logical_start + index + 1;
      memmove(bt_cur, logical_end, (bt->last - logical_end + 1)*sizeof(frame_t));
      bt->last = bt_cur + (bt->last - logical_end);
    }
    off = REGIONS_PER_SEGMENT;
  }

earlyexit:
  if(seg != NULL) {
    TMSG(LOGICAL_UNWIND, "WARNING: The following logical regions did not match: (partial: %s)", bt->partial_unwind ? "yes" : "no");
    for(; seg != NULL; seg = seg->prev) {
      for(; off > 0; off--) {
        logical_region_t* cur = &seg->regions[off-1];
        TMSG(LOGICAL_UNWIND, " sp @ exit = %p, sp @ beforeenter = %p",
             cur->exit, cur->beforeenter.sp);
      }
    }
  }

  for(; bt_cur != bt->last; bt_cur++)
    TMSG(LOGICAL_UNWIND, " sp = %p  ip = %s +%p", bt_cur->cursor.sp,
          name_for(bt_cur->ip_norm.lm_id), bt_cur->ip_norm.lm_ip);
  TMSG(LOGICAL_UNWIND, " sp = %p  ip = %s +%p", bt_cur->cursor.sp,
        name_for(bt_cur->ip_norm.lm_id), bt_cur->ip_norm.lm_ip);

  TMSG(LOGICAL_UNWIND, "========= END Logicalizing backtrace =========");
}

// ---------------------------------------
// Logical metadata mangagement
// ---------------------------------------

void hpcrun_logical_metadata_register(logical_metadata_store_t* store, const char* generator) {
  spinlock_init(&store->lock);
  store->nextid = 1;  // 0 is reserved for the logical unknown
  store->idtable = NULL;
  store->tablesize = 0;
  store->generator = generator;
  atomic_init(&store->lm_id, 0);
  store->path = NULL;
  store->next = metadata;
  metadata = store;
}

void hpcrun_logical_metadata_generate_lmid(logical_metadata_store_t* store) {
  // Storage for the path we will be generating
  // <output dir> + /logical/ + <generator> + . + <8 random hex digits> + \0
  store->path = hpcrun_malloc(strlen(hpcrun_files_output_directory())
                              + 9 + strlen(store->generator) + 1 + 8 + 1);
  if(store->path == NULL)
    hpcrun_abort("hpcrun: error allocating space for logical metadata path");

  // First make sure the directory is created
  char* next = store->path+sprintf(store->path, "%s/logical", hpcrun_files_output_directory());
  int ret = mkdir(store->path, 0755);
  if(ret != 0 && errno != EEXIST) {
    hpcrun_abort("hpcrun: error creating logical metadata output directory `%s`: %s",
                 store->path, strerror(errno));
  }

  // Then create the file where all the bits will be dumped
  next += sprintf(next, "/%s.", store->generator);
  int fd = -1;
  do {
    // The last part of the name is just randomly generated until no conflict
    // <50% chance of conflict until ~5 billion metadata files
    sprintf(next, "%08lx", random());
    fd = open(store->path, O_WRONLY | O_EXCL | O_CREAT, 0644);
    if(fd == -1) {
      if(errno == EEXIST) continue;  // Try again
      hpcrun_abort("hpcrun: error creating logical metadata output `%s`: %s",
                   store->path, strerror(errno));
    }
    break;
  } while(1);
  close(fd);

  // Register the path with the loadmap
  atomic_store_explicit(&store->lm_id, hpcrun_loadModule_add(store->path), memory_order_release);
}

// Roughly the FNV-1a hashing algorithm, simplified slightly for ease of use
static size_t string_hash(const char* data) {
  size_t sponge = (size_t)0x2002100120021001ULL;
  if(data == NULL) return sponge;
  const size_t sz = strlen(data);
  const size_t prime = _Generic((sponge),
      uint32_t: UINT32_C(0x01000193),
      uint64_t: UINT64_C(0x00000100000001b3));
  size_t i;
  for(i = 0; i < sz; i += sizeof sponge) {
    sponge ^= *(size_t*)&data[i];
    sponge *= prime;
  }
  i -= sizeof sponge;
  size_t last = (size_t)0x1000000110000001ULL;
  memcpy(&last, data+i, sz-i);
  return (sponge ^ last) * prime;
}

// Integer mixer from https://stackoverflow.com/a/12996028
static size_t int_hash(uint32_t x) {
  x = ((x >> 16) ^ x) * UINT32_C(0x45d9f3b);
  x = ((x >> 16) ^ x) * UINT32_C(0x45d9f3b);
  x = (x >> 16) ^ x;
  return _Generic((size_t)0,
    uint32_t: x,
    uint64_t: ((uint64_t)x << 32) | x);
}

static struct logical_metadata_store_entry_t* hashtable_probe(
    logical_metadata_store_t* store, const struct logical_metadata_store_entry_t* needle) {
  for(size_t i = 0; i < store->tablesize/2; i++) {
    // Simple quadratic probing scheme
    struct logical_metadata_store_entry_t* entry =
        &store->idtable[(needle->hash+i*i) & (store->tablesize-1)];

    if(entry->id == 0) {
      // We hit an empty entry, so it must not be in the table.
      return entry;
    }

    // Check all the cheap comparisons first
    if(needle->hash != entry->hash) continue;
    if((needle->funcname == NULL) != (entry->funcname == NULL)) continue;
    if((needle->filename == NULL) != (entry->filename == NULL)) continue;
    if(needle->lineno != entry->lineno) continue;

    // Then do the expensive string comparisons
    if(needle->funcname != NULL && strcmp(needle->funcname, entry->funcname) != 0)
      continue;
    if(needle->filename != NULL && strcmp(needle->filename, entry->filename) != 0)
      continue;

    // Everything matches, we found it!
    return entry;
  }

  // Ran out of probes, we should have found it by now. Must not be here.
  return NULL;
}

static void hashtable_grow(logical_metadata_store_t* store) {
  if(store->idtable == NULL) {
    // If the table hasn't been created yet, start off with 256 entries
    store->tablesize = 1<<8;
    store->idtable = hpcrun_malloc(store->tablesize * sizeof store->idtable[0]);
    memset(store->idtable, 0, store->tablesize * sizeof store->idtable[0]);
    return;
  }

  // Otherwise, grow the table by 4x (to lower the number of times we grow)
  size_t oldsize = store->tablesize;
  struct logical_metadata_store_entry_t* oldtable = store->idtable;
  store->tablesize *= 4;
  store->idtable = hpcrun_malloc(store->tablesize * sizeof store->idtable[0]);
  memset(store->idtable, 0, store->tablesize * sizeof store->idtable[0]);

  // Copy all the non-empty entries from the old table into the new one
  for(size_t i = 0; i < oldsize; i++) {
    if(oldtable[i].id != 0) {
      struct logical_metadata_store_entry_t* e = hashtable_probe(store, &oldtable[i]);
      assert(e != NULL && "Failure while repopulating hash table!");
      *e = oldtable[i];
    }
  }

  // We should free oldtable here, but hpcrun_malloc memory can't be freed...
}

uint32_t hpcrun_logical_metadata_fid(logical_metadata_store_t* store,
    const char* funcname, const char* filename, uint32_t lineno) {
  if(funcname == NULL && filename == NULL)
    return 0; // Specially reserved for this case
  if(funcname == NULL) lineno = 0;  // It should be ignored

  spinlock_lock(&store->lock);

  // We're looking for an entry that looks roughly like this
  struct logical_metadata_store_entry_t pattern = {
    .funcname = (char*)funcname, .filename = (char*)filename, .lineno = lineno,
    .hash = string_hash(funcname) ^ string_hash(filename) ^ int_hash(lineno),
  };

  // Probe for the entry, or at least where it should be.
  struct logical_metadata_store_entry_t* entry = hashtable_probe(store, &pattern);
  if(entry != NULL && entry->id != 0) {
    // Found it! We're done here.
    spinlock_unlock(&store->lock);
    return entry->id;
  }
  if(entry == NULL) {
    // We didn't find it, but we also didn't find an empty slot to put the new
    // entry. Grow the table and re-probe to get an empty space for us.
    hashtable_grow(store);
    entry = hashtable_probe(store, &pattern);
    assert(entry != NULL && "Entry still not found after growth!");
  }

  // This is a brand new entry. Copy most fields, and make copies of the strings
  // to ensure someone doesn't accidentally free the memory out from under us.
  *entry = pattern;
  entry->id = store->nextid++;
  if(entry->funcname != NULL) {
    const char* base = entry->funcname;
    entry->funcname = hpcrun_malloc(strlen(base)+1);
    strcpy(entry->funcname, base);
  }
  if(entry->filename != NULL) {
    const char* base = entry->filename;
    entry->filename = hpcrun_malloc(strlen(base)+1);
    strcpy(entry->filename, base);
  }

  spinlock_unlock(&store->lock);
  return entry->id;
}

static void cleanup_metadata_store(logical_metadata_store_t* store) {
  FILE* f = fopen(store->path, "wb");
  if(f == NULL) return;
  fprintf(f, "HPCLOGICAL");
  for(size_t idx = 0; idx < store->tablesize; idx++) {
    struct logical_metadata_store_entry_t* entry = &store->idtable[idx];
    if(entry->id == 0) continue;
    hpcfmt_int4_fwrite(entry->id, f);
    hpcfmt_str_fwrite(entry->funcname, f);
    hpcfmt_str_fwrite(entry->filename, f);
    hpcfmt_int4_fwrite(entry->lineno, f);
  }
  fclose(f);
}
