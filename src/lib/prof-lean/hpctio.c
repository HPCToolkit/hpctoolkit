#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

#include "hpctio.h"
#include "hpctio_obj.h"


///////////////////////////////////////HPCTIO_SYS FUNCTIONS///////////////////////////////////////////////

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
#if OPT_ENABLE_DAOS
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

    }
#endif

    //return the posix one
    return hpctio_sys_avail[0];
    
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
        printf("Object %d - %ld:\n", i, (intptr_t)sys);
        #if OPT_ENABLE_DAOS
        if(sys->func_ptr == &hpctio_sys_func_dfs){
            printf("File system: DAOS\n");
            hpctio_sys_func_dfs.display_params(sys->params_ptr);
        }else 
        #endif
        if(sys->func_ptr == &hpctio_sys_func_posix){
            printf("File system: POSIX\n");
            printf("No parameters for POSIX\n");
        }else{
            printf("ERROR: Unknown File System function pointer\n");
        }
    }
    if(hpctio_sys_count < 4) printf("Last one %ld\n", (intptr_t)hpctio_sys_avail[hpctio_sys_count]);
    printf("---------------------------------------------\n");
}

/*
* return the given path without file system prefixes
*/
char * hpctio_sys_cut_prefix(const char * path, hpctio_sys_t * sys){
    return sys->func_ptr->cut_prefix(path);
}

/*
* return the realpath 
* POSIX: the absolute full path
* DAOS: the given path is the realpath
*/
char * hpctio_sys_realpath(const char * path, char * resolved_path, hpctio_sys_t * sys){
    return sys->func_ptr->real_path(path, resolved_path);
}


/*
* make a diretcory with path and mode specified
* return 0 on success, -1 on failure, errno set properly
*/
int hpctio_sys_mkdir(const char *path, mode_t md, hpctio_sys_t * sys){
    return sys->func_ptr->mkdir(path, md, sys->params_ptr);
}

/*
* Deletes a file/object with a path name
* return 0 on success, -1 on failure, errno set properly
*/
int hpctio_sys_remove(const char *path, hpctio_sys_t * sys){
    int r = sys->func_ptr->remove(path, sys->params_ptr);  
    return r;
}


/*
* check access permissions on a file/object
* return 0 on success, -1 on failure, errno set properly
*/
int hpctio_sys_access(const char *path, int md, hpctio_sys_t * sys){
    return sys->func_ptr->access(path, md, sys->params_ptr);
}

/*
* rename a file/object from old_path to new_path
* return 0 on success, -1 on failure, errno set properly
*/
int hpctio_sys_rename(const char *oldpath, const char *newpath, hpctio_sys_t * sys){
    return sys->func_ptr->rename(oldpath, newpath, sys->params_ptr);
}

/*
* check stat attributes of a path entry
* return 0 on success, -1 on failure, errno set properly
*/
int hpctio_sys_stat(const char *path, struct stat * stbuf, hpctio_sys_t * sys){
    return sys->func_ptr->stat(path, stbuf, sys->params_ptr);
}

/*
* return a list of entries in a directory 
* return entries name in second argument, number of entries as output
*/
int hpctio_sys_readdir(const char* path, char *** entries, hpctio_sys_t * sys){
    return sys->func_ptr->readdir(path, entries, sys->params_ptr);
}


///////////////////////////////////////HPCTIO_OBJ FUNCTIONS///////////////////////////////////////////////
/*
* Open/create a file/object with path and flags
* return hpctio_obj_t * on success, NULL on failure, errno set properly
*/
hpctio_obj_t * hpctio_obj_open(const char *path, int flags, mode_t md, int wmode, int sizetype, hpctio_sys_t * sys){
    hpctio_obj_t * obj = (hpctio_obj_t *) malloc(sizeof(hpctio_obj_t));
    
    obj->sys_ptr = sys;

    hpctio_obj_opt_t * opt = sys->func_ptr->obj_options(wmode, sizetype);
    obj->opt_ptr = opt;

    hpctio_obj_id_t * oid = sys->func_ptr->open(path, flags, md, opt, sys->params_ptr);
    obj->oh = oid;

    if(oid == NULL){
        free(obj);
        obj = NULL;
    }
    
    return obj;
}

/*
* Closes a file/object with hpctio_obj_t *
* return 0 on success, -1 on failure, errno set properly
*/
int hpctio_obj_close(hpctio_obj_t * obj){
    int r = obj->sys_ptr->func_ptr->close(obj->oh, obj->opt_ptr, obj->sys_ptr->params_ptr);  
    if(r == 0){
        free(obj->opt_ptr);
        obj->opt_ptr = NULL;
        free(obj);
    } 
    return r;
}


/*
* Append to a file 
* return the number of elements written if succeed, -1  on failure with errno set
*/
size_t hpctio_obj_append(const void * buf, size_t size, size_t nitems, hpctio_obj_t * obj){
    return obj->sys_ptr->func_ptr->append(buf, size, nitems, obj->oh, obj->opt_ptr, obj->sys_ptr->params_ptr);
}


/*
* Writeat to a file 
* return the number of elements written if succeed, -1  on failure with errno set
*/
size_t hpctio_obj_writeat(const void * buf, size_t count, uint64_t offset, hpctio_obj_t * obj){
    return obj->sys_ptr->func_ptr->writeat(buf, count, offset, obj->oh, obj->opt_ptr, obj->sys_ptr->params_ptr);
}

/*
* Prefetch a block of data from a file and store it in a read buffer
* return 0 if succeed, -1  on failure
*/
int hpctio_obj_prefetch(uint64_t startoff, uint64_t endoff, hpctio_obj_t * obj){
   return obj->sys_ptr->func_ptr->prefetch(startoff, endoff, obj->oh, obj->opt_ptr, obj->sys_ptr->params_ptr);
}


/*
* Readat to a file 
* return the number of elements written if succeed, -1  on failure with errno set
*/
size_t hpctio_obj_readat(void * buf, size_t count, uint64_t offset, hpctio_obj_t * obj){
    return obj->sys_ptr->func_ptr->readat(buf, count, offset, obj->oh, obj->opt_ptr, obj->sys_ptr->params_ptr);
}


/*
* Tell the current cursor position of the object
* return the cursor position on success, -1 on failure with sometimes errno set
*/
long int hpctio_obj_tell(hpctio_obj_t * obj){
    return obj->sys_ptr->func_ptr->tell(obj->oh, obj->opt_ptr);
}

