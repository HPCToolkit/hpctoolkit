#ifndef HPCTIO_OBJ_H
#define HPCTIO_OBJ_H

#include "hpctio.h"

/*************************** FILE SYSTEM OBJECT STRUCTS ***************************/
typedef struct hpctio_obj{
    // pointer to file system objects
    hpctio_sys_t * sys_ptr;

    // pointer to object options 
    hpctio_obj_opt_t * opt_ptr;

    //object handle
    hpctio_obj_id_t * oh; 
}hpctio_obj_t;

/*************************** FILE SYSTEM OBJECT WRITING MODES ***************************/
#define HPCTIO_WRITE_ONLY  0
#define HPCTIO_APPEND_ONLY 1
#define HPCTIO_WRITAPPEND  2

/*************************** FILE SYSTEM OBJECT FUNCTIONS ***************************/
hpctio_obj_t * hpctio_obj_open(const char *path, int flags, mode_t md, int writemode, hpctio_sys_t * sys);
int hpctio_obj_close(hpctio_obj_t * obj);
size_t hpctio_obj_append(const void * buf, size_t size, size_t nitems, hpctio_obj_t * obj);
long int hpctio_obj_tell(hpctio_obj_t * obj);

#endif /* ifndef HPCTIO_OBJ_H */