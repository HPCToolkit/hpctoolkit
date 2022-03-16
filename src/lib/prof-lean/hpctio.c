#include "hpctio.h"
#include <string.h>
#include <stdlib.h>

// define global I/O constants
const char *  hpctio_daos_prefix = "daos://";

// assume won't be having more than 4 system objects at a time
static hpctio_sys_t * hpctio_sys_avail[4] = {&hpctio_sys_posix};
static int hpctio_sys_count = 1;


//TODO: need a lock?
/*
* return related hpctio_sys_t object
* create a new one and insert it into hpctio_sys_avail if not already existed
*/
hpctio_sys_t * hpctio_sys_initialize(const char * path){
    // DAOS:
    if(strncmp(hpctio_daos_prefix, path, strlen(hpctio_daos_prefix)) == 0){
        // check if already initialized
        for(int i = 0; i < hpctio_sys_count; i++){
            if(hpctio_sys_avail[i]->func_ptr != &hpctio_sys_func_dfs)
                continue;
            
            // the system object with the same pool and container exists 
            if(hpctio_sys_func_dfs.compare_params(path, hpctio_sys_avail[i]->params_ptr) == 0)
                return hpctio_sys_avail[i];
        }

        // we need to create a new system object
        hpctio_sys_t * new_sys = (hpctio_sys_t *)malloc(sizeof(hpctio_sys_t));
        new_sys->func_ptr = &hpctio_sys_func_dfs;
        new_sys->params_ptr = hpctio_sys_func_dfs.construct_params(path);

        // append the new system object into hpctio_sys_avail
        hpctio_sys_avail[hpctio_sys_count] = new_sys;
        hpctio_sys_count++;

        // return the new object
        return new_sys;

    }else{
        //return the posix one
        return hpctio_sys_avail[0];
    }
}

//TODO: need a lock?
/*
* finalize and free the system object
* update hpctio_sys_avail
*/
void hpctio_sys_finalize(hpctio_sys_t * sys){
    // keep POSIX system object always in hpctio_sys_avail
    if(sys == &hpctio_sys_posix) 
        return;

    for(int i = 0; i < hpctio_sys_count; i++){
        if(hpctio_sys_avail[i] == sys){
            sys->func_ptr->finalize(sys->params_ptr);
            free(sys);
            if(i != hpctio_sys_count - 1){
                hpctio_sys_avail[i] = hpctio_sys_avail[hpctio_sys_count - 1];
            }else{
                hpctio_sys_avail[i] = NULL;
            }

            hpctio_sys_count--;
            break;
        }       
    }
}

