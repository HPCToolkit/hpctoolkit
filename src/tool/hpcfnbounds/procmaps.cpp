
//******************************************************************************
// global includes
//******************************************************************************

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <sys/param.h>


//******************************************************************************
// local includes
//******************************************************************************



//******************************************************************************
// types
//******************************************************************************

typedef enum {
  lm_perm_r = 1,
  lm_perm_w = 2, 
  lm_perm_x = 4, 
  lm_perm_p = 8 
} lm_perm_t;


typedef struct lm_seg_s {
  void* start_address;      // segment start address   
  void* end_address;        // segment end address
  int permissions;
  unsigned long offset; 
  char device[32];          // major:minor
  int inode;                // inode of the file that backs the area
  char path[MAXPATHLEN];    // the path to the file associated with the segment
} lm_seg_t;



//******************************************************************************
// private operations
//******************************************************************************

static int
lm_segment_permissions_decode(char *perms)
{
  int result = 0;
  while(*perms) {
    switch(*perms) {
    case 'r': result |= lm_perm_r; break;
    case 'w': result |= lm_perm_w; break;
    case 'x': result |= lm_perm_x; break;
    case 'p': result |= lm_perm_p; break;
    default: break;
    }
    perms++;
  }
  return result;
}

const char *
lm_segment_path(lm_seg_t *s)
{
  return s->path;
}

static bool 
lm_segment_contains
(
  lm_seg_t *s,
  void *addr
)
{
  return (s->start_address <= addr) && (addr < s->end_address);
}

void
lm_segment_parse
(
  lm_seg_t *s,
  const char *line
)
{
  char item[MAXPATHLEN];
  s->path[0] ='\0';
  sscanf(line, "%p-%p %s %lx %s %d %s", &s->start_address, &s->end_address, item, 
	 &s->offset, s->device, &s->inode, s->path);
  s->permissions = lm_segment_permissions_decode(item);
}
