#ifndef _FNBOUNDS_FILE_HEADER_
#define _FNBOUNDS_FILE_HEADER_

#include <sys/types.h>

// Extra fnbounds info beyond the array of addresses.  This is used
// inside hpcrun between fnbounds and the loadmap.
//
// Note: this struct does not need to be platform-independent because
// we never write it to a file or share it across machines.  In the
// static case, we write these fields as source code and the compiler
// generates the correct format.

struct fnbounds_file_header {
  unsigned long  num_entries;
  unsigned long  reference_offset;
  int     is_relocatable;
  size_t  mmap_size;
};

#endif
