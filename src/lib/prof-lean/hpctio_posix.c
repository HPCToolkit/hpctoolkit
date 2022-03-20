#include "hpctio.h"
#include <stdio.h>


/************************** PROTOTYPES *****************************/
static char * POSIX_Realpath(const char * path);
static void POSIX_Init(hpctio_sys_params_t * params);
static void POSIX_Final(hpctio_sys_params_t * params);
static int POSIX_Mkdir(const char *path, mode_t md, hpctio_sys_params_t * p);


/*************************** FILE SYSTEM STRUCTS ***************************/
//file system parameters struct - only use one POSIX, no struct to define "each" 

//file system functions struct
hpctio_sys_func_t hpctio_sys_func_posix = {
  .construct_params = NULL,
  .compare_params = NULL,
  .display_params = NULL,
  .real_path = POSIX_Realpath,
  .initialize = POSIX_Init,
  .finalize   = POSIX_Final,
  .mkdir = POSIX_Mkdir



};

hpctio_sys_t hpctio_sys_posix = {
  .func_ptr = &hpctio_sys_func_posix,
  .params_ptr = NULL
};

/*************************** FILE SYSTEM OBJECT STRUCTS ***************************/
//file system objects options struct
typedef struct hpctio_posix_obj_opt {
  //TODO: Lustre varaibles here
}hpctio_posix_obj_opt_t;

//file system object struct
typedef struct hpctio_posix_fd {
  int fd;
  
}hpctio_posix_fd_t;


/************************** FUNCTIONS *****************************/
static void POSIX_Init(hpctio_sys_params_t * params){}
static void POSIX_Final(hpctio_sys_params_t * params){}

static char * POSIX_Realpath(const char * path){
  return path;
}

static int POSIX_Mkdir(const char *path, mode_t md, hpctio_sys_params_t * p){
  mkdir(path, md);
}