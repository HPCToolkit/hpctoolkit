#ifndef HPCTIO_OBJ_H
#define HPCTIO_OBJ_H

#include "hpctio.h"

#ifdef __cplusplus
extern "C" {
#endif

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
#define HPCTIO_WRITE_AT  0 
#define HPCTIO_APPEND    1 

#define HPCTIO_LARGE_F   0
#define HPCTIO_SMALL_F   1
/*************************** FILE SYSTEM OBJECT FUNCTIONS ***************************/
hpctio_obj_t * hpctio_obj_open(const char *path, int flags, mode_t md, int wmode, int sizetype, hpctio_sys_t * sys);
int hpctio_obj_close(hpctio_obj_t * obj);

size_t hpctio_obj_append(const void * buf, size_t size, size_t nitems, hpctio_obj_t * obj);
size_t hpctio_obj_writeat(const void * buf, size_t count, uint64_t offset, hpctio_obj_t * obj);
size_t hpctio_obj_readat(void * buf, size_t count, uint64_t offset, hpctio_obj_t * obj);

long int hpctio_obj_tell(hpctio_obj_t * obj);

//int hpctio_obj_filesize(hpctio_obj_t * obj);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ifndef HPCTIO_OBJ_H */