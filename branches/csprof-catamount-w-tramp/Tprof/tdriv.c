#include "papi_pulse.h"

void
t_driver_init(void){
    setup_segv();
    papi_pulse_init();
}

void t_driver_fini(void){
  papi_pulse_fini();
}
