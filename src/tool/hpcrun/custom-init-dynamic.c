//
// run a special user initialization function, if the CUSTOM_INIT flag is set
//
// Dynamic case uses dl functions to open a canonically named shared object file.
// extract the "hpcrun_custom_init" function, and call it.
//
// The canonical name for the shared object is "hpcrun-custom.so". At the moment, this
// file must be found in the same directory as the one in which hpcrun is invoked.
// 
//

// *********************************************
// system includes
// *********************************************

#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <dlfcn.h>

// *********************************************
// local includes
// *********************************************

#include "custom-init.h"
#include <monitor.h>
#include <messages/messages.h>

// *********************************************
// interface functions
// *********************************************

void
hpcrun_do_custom_init(void)
{
  if (ENABLED(CUSTOM_INIT)) {
    void* custom_fns = monitor_real_dlopen("./hpcrun-custom.so", RTLD_LAZY);
    if (custom_fns) {
      void (*hpcrun_custom_init)(void) = (void (*)(void)) dlsym(custom_fns, "hpcrun_custom_init");
      if (hpcrun_custom_init) {
	TMSG(CUSTOM_INIT, "Before call to custom_init");
	hpcrun_custom_init();
	TMSG(CUSTOM_INIT, "Return from custom_init");
      }
      else {
	TMSG(CUSTOM_INIT, "could not dynamically load hpcrun_custom_init procedure");
      }
      monitor_real_dlclose(custom_fns);
    }
    else {
      TMSG(CUSTOM_INIT, "could not open hpcrun-custom.so");
    }
  }
}
