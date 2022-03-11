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

/*************************** FILE SYSTEM OBJECT FUNCTIONS ***************************/




#endif /* ifndef HPCTIO_OBJ_H */