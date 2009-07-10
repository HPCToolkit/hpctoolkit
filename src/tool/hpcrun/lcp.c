//
// A loadmap, cct "pair" (will eventually become an epoch)
//
//    a cct, at the moment, is a creation context coupled with a normal cct
//

#include <sys/time.h>
#include <stdlib.h>

#include "lcp.h"
#include "pmsg.h"
#include "sample_event.h"

static csprof_epoch_t _loadmap;

static lcp_t _lcp = {
  .loadmap = &_loadmap,
  .cct     = NULL,
  .ctxt    = NULL,
};

void
hpcrun_lcp_loadmap_init(lcp_t* lcp)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  csprof_epoch_t* e = lcp->loadmap;

  TMSG(LCP, "lcp loadmap init");
  TMSG(LCP, "new loadmap time: sec = %ld, usec = %d, samples = %ld",
       (long)tv.tv_sec, (int)tv.tv_usec, csprof_num_samples_total());

  memset(e, 0, sizeof(*e));

  e->loaded_modules = NULL;
}

lcp_t*
hpcrun_static_lcp(void)
{
  TMSG(LCP,"returning static lcp");
  return &_lcp;
}

lcp_t*
hpcrun_lcp_new(void)
{
  TMSG(LCP,"create new lcp");
  lcp_t* rv = (lcp_t*) csprof_malloc(sizeof(lcp_t));
  rv->loadmap = csprof_epoch_new();
  // cct_new
  // ctxt = NULL

  return rv;
}
