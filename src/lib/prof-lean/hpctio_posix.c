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
static int POSIX_Stat(const char* path, struct stat * stbuf, hpctio_sys_params_t * p);

static hpctio_obj_opt_t * POSIX_Obj_Options(int wmode, int sizetype);
static hpctio_obj_id_t * POSIX_Open(const char * path, int flags, mode_t md, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p);
static int POSIX_Close(hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p);


static size_t POSIX_Append(const void * buf, size_t size, size_t nitems, hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p);
static long int POSIX_Tell(hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt);

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
  .stat = POSIX_Stat,
  .obj_options = POSIX_Obj_Options,
  .open = POSIX_Open,
  .close = POSIX_Close,
  .append = POSIX_Append,
  .tell = POSIX_Tell,
  .readdir = NULL


};

hpctio_sys_t hpctio_sys_posix = {
  .func_ptr = &hpctio_sys_func_posix,
  .params_ptr = NULL
};

/*************************** FILE SYSTEM OBJECT STRUCTS ***************************/
//file system objects options struct
typedef struct hpctio_posix_obj_opt {
  //TODO: Lustre varaibles here
  int wmode;
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

static int POSIX_Stat(const char* path, struct stat * stbuf, hpctio_sys_params_t * p){
  return stat(path, stbuf);
}


static hpctio_obj_opt_t * POSIX_Obj_Options(int wmode, int sizetype){
  hpctio_posix_obj_opt_t  * opt = (hpctio_posix_obj_opt_t  *) malloc(sizeof(hpctio_posix_obj_opt_t ));
  opt->wmode = wmode;
  return (hpctio_obj_opt_t *)opt;
}


static hpctio_obj_id_t * POSIX_Open(const char * path, int flags, mode_t md, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p){
  hpctio_posix_fd_t * obj = (hpctio_posix_fd_t *) malloc(sizeof(hpctio_posix_fd_t));
  hpctio_posix_obj_opt_t * popt = (hpctio_posix_obj_opt_t *) opt;
  int r = 0;

  int fd;
  if(md){
    fd = open(path, flags, md);
  }else{
    fd = open(path, flags);
  }
  r = (fd < 0);
  CHECK(r, "POSIX_Open failed to open the file descriptor for file %s", path);

  if(popt->wmode == HPCTIO_APPEND){
    obj->fd = fd;
    obj->fs = fdopen(fd, "w");
    r = (obj->fs == NULL);
    CHECK(r, "POSIX_Open failed to fdopen the file stream");
  }else{ // HPCTIO_WRITE_AT
    obj->fd = fd;
    obj->fs = NULL; 
  }

exit:
  if(r){
    free(obj);
    obj = NULL;
  } 
  return (hpctio_obj_id_t *)obj;
}

static int POSIX_Close(hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p){
  hpctio_posix_fd_t * pobj = (hpctio_posix_fd_t *) obj;
  int r;

  if(pobj->fs){
    r = fclose(pobj->fs);
    CHECK(r,  "POSIX - Failed to fclose the file stream");
  }else{
    r = close(pobj->fd);
    CHECK(r,  "POSIX - Failed to close the file descriptor");
  }

  pobj->fs = NULL;
  pobj->fd = NULL;
  free(pobj);

exit:
  return r;
}

static size_t POSIX_Append(const void * buf, size_t size, size_t nitems, hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p){
  hpctio_posix_obj_opt_t * popt = (hpctio_posix_obj_opt_t *) opt;
  CHECK((popt->wmode != HPCTIO_APPEND),  "POSIX - Appending to a file without HPCTIO_APPEND mode is not allowed");

  hpctio_posix_fd_t * pfd = (hpctio_posix_fd_t *) obj;
  size_t s = fwrite(buf, size, nitems, pfd->fs);

exit:
  if(popt->wmode != HPCTIO_APPEND){
    return -1;
  }else{
    return s == nitems ? s : -1;
  }
  
}

static long int POSIX_Tell(hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt){
  hpctio_posix_fd_t * pfd = (hpctio_posix_fd_t *) obj;
  hpctio_posix_obj_opt_t * popt = (hpctio_posix_obj_opt_t *) opt;

  if(popt->wmode == HPCTIO_WRITE_AT || pfd->fs == NULL){
    return -1;
  }

  int r = ftell(pfd->fs);
  return r;
}


