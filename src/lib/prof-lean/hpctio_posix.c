#include <errno.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "hpctio.h"
#include "hpctio_obj.h"


/************************** PROTOTYPES *****************************/
static char * POSIX_Cut_Prefix(const char * path);
static char * POSIX_Realpath(const char * path, char * resolved_path);
static void POSIX_Init(hpctio_sys_params_t * params);
static void POSIX_Final(hpctio_sys_params_t * params);

static int POSIX_Mkdir(const char *path, mode_t md, hpctio_sys_params_t * p);
static int POSIX_Access(const char *path, int md, hpctio_sys_params_t * p);
static int POSIX_Rename(const char *oldpath, const char *newpath, hpctio_sys_params_t * p);

static hpctio_obj_opt_t * POSIX_Obj_Options(int writemode);
static hpctio_obj_id_t * POSIX_Open(const char * path, int flags, mode_t md, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p);
static int POSIX_Close(hpctio_obj_id_t * obj);

static size_t POSIX_Append(const void * buf, size_t size, size_t nitems, hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p);


/*************************** FILE SYSTEM STRUCTS ***************************/
//file system parameters struct - only use one POSIX, no struct to define "each" 

//file system functions struct
hpctio_sys_func_t hpctio_sys_func_posix = {
  .construct_params = NULL,
  .compare_params = NULL,
  .display_params = NULL,
  .cut_prefix = POSIX_Cut_Prefix,
  .real_path = POSIX_Realpath,
  .initialize = POSIX_Init,
  .finalize   = POSIX_Final,
  .mkdir = POSIX_Mkdir,
  .access = POSIX_Access,
  .rename = POSIX_Rename,
  .obj_options = POSIX_Obj_Options,
  .open = POSIX_Open,
  .close = POSIX_Close,
  .append = POSIX_Append


};

hpctio_sys_t hpctio_sys_posix = {
  .func_ptr = &hpctio_sys_func_posix,
  .params_ptr = NULL
};

/*************************** FILE SYSTEM OBJECT STRUCTS ***************************/
//file system objects options struct
typedef struct hpctio_posix_obj_opt {
  //TODO: Lustre varaibles here
  int writemode;
  //int cursor;
}hpctio_posix_obj_opt_t;

//file system object struct
typedef struct hpctio_posix_fd {
  int fd;
  FILE * fs;
}hpctio_posix_fd_t;


/************************** FUNCTIONS *****************************/
#define CHECK(r, msg, ...)                           \
{                                                    \
    if(r != 0) {                                     \
        fprintf(stderr, "ERROR (%s:%d): "msg"\n",   \
             __FILE__, __LINE__, ##__VA_ARGS__);     \
        fflush(stderr);                              \
        goto exit;                                   \
    }                                                \
}    




static void POSIX_Init(hpctio_sys_params_t * params){}
static void POSIX_Final(hpctio_sys_params_t * params){}

static char * POSIX_Cut_Prefix(const char * path){
  return strdup(path);
}

static char * POSIX_Realpath(const char * path, char * resolved_path){
  char* rpath = realpath(path, resolved_path);
  return rpath;
}

static int POSIX_Mkdir(const char *path, mode_t md, hpctio_sys_params_t * p){
  return mkdir(path, md);
}

static int POSIX_Access(const char *path, int md, hpctio_sys_params_t * p){
  return access(path, md);
}

static int POSIX_Rename(const char *oldpath, const char *newpath, hpctio_sys_params_t * p){
  return rename(oldpath, newpath);
}


static hpctio_obj_opt_t * POSIX_Obj_Options(int writemode){
  hpctio_posix_obj_opt_t  * opt = (hpctio_posix_obj_opt_t  *) malloc(sizeof(hpctio_posix_obj_opt_t ));
  opt->writemode = writemode;
  return (hpctio_obj_opt_t *)opt;
}


static hpctio_obj_id_t * POSIX_Open(const char * path, int flags, mode_t md, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p){
  hpctio_posix_fd_t * obj = (hpctio_posix_fd_t *) malloc(sizeof(hpctio_posix_fd_t));
  hpctio_posix_obj_opt_t * popt = (hpctio_posix_obj_opt_t *) opt;

  int fd;
  if(md){
    fd = open(path, flags, md);
  }else{
    fd = open(path, flags);
  }

  if(fd < 0){
    free(obj);
    obj = NULL;
  }else if(popt->writemode == HPCTIO_APPEND_ONLY || 
      popt->writemode == HPCTIO_WRITAPPEND){
    obj->fd = fd;
    obj->fs = fdopen(fd, "w");
    //popt->cursor = 0;
  }

  return (hpctio_obj_id_t *)obj;
}

static int POSIX_Close(hpctio_obj_id_t * obj){
  hpctio_posix_fd_t * pobj = (hpctio_posix_fd_t *) obj;
  int r;
  r = fclose(pobj->fs);
  CHECK(r,  "POSIX - Failed to fclose the file stream");

  r = close(pobj->fd);
  CHECK(r,  "POSIX - Failed to fclose the file descriptor");

  pobj->fs = NULL;
  pobj->fd = NULL;
  free(pobj);

exit:
  if((r != 0) && (r != EOF)){
    errno = r;
  }
  if(r) r = -1;

  return r;
}

size_t POSIX_Append(const void * buf, size_t size, size_t nitems, hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p){
  hpctio_posix_obj_opt_t * popt = (hpctio_posix_obj_opt_t *) opt;
  CHECK((popt->writemode == HPCTIO_WRITE_ONLY),  "POSIX - Appending to a file with HPCTIO_WRITE_ONLY mode is not allowed");

  hpctio_posix_fd_t * pfd = (hpctio_posix_fd_t *) obj;
  size_t s = fwrite(buf, size, nitems, pfd->fs);
  CHECK((s != nitems),  "POSIX - an error occurs during fwrite with errno %d", errno);

exit:
  return s == nitems ? s : -1;
}