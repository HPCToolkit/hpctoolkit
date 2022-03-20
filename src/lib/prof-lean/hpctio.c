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

        // initialize the object
        new_sys->func_ptr->initialize(new_sys->params_ptr);

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


/*
* return a default system object
*/
hpctio_sys_t * hpctio_sys_default(){
    return hpctio_sys_avail[0]; //default system is always POSIX
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
            }
            hpctio_sys_avail[hpctio_sys_count - 1] = NULL;        
            hpctio_sys_count--;
            break;
        }       
    }
}

void hpctio_sys_avail_display(){
    printf("---------------------------------------------\n");
    printf("Available/Initialized %d file system objects:\n", hpctio_sys_count);
    for(int i = 0; i < hpctio_sys_count; i++){
        hpctio_sys_t * sys = hpctio_sys_avail[i];
        printf("Object %d - %d:\n", i, sys);
        if(sys->func_ptr == &hpctio_sys_func_dfs){
            printf("File system: DAOS\n");
            hpctio_sys_func_dfs.display_params(sys->params_ptr);
        }else if(sys->func_ptr == &hpctio_sys_func_posix){
            printf("File system: POSIX\n");
            printf("No parameters for POSIX\n");
        }else{
            printf("ERROR: Unknown File System function pointer\n");
        }
    }
    if(hpctio_sys_count < 4) printf("Last one %s\n", hpctio_sys_avail[hpctio_sys_count]);
    printf("---------------------------------------------\n");
}

/*
* return the path without file system prefixes
*/
char * hpctio_sys_path(const char * path, hpctio_sys_t * sys){
    return sys->func_ptr->real_path(path);
}


/*
* make a diretcory with path and mode specified
* return 0 on success, -1 on failure, errno set properly
*/
int hpctio_sys_mkdir(const char *path, mode_t md, hpctio_sys_t * sys){
    return sys->func_ptr->mkdir(path, md, sys->params_ptr);
}