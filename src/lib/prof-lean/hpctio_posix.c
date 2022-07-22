#include <errno.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#ifdef HAVE_LINUX_LUSTRE_USER_H
#  include <linux/lustre/lustre_user.h>
#elif defined(HAVE_LUSTRE_USER_H)
#  include <lustre/lustre_user.h>
#endif

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
static size_t POSIX_Writeat(const void * buf, size_t count, uint64_t offset, hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p);
static int POSIX_Prefetch(uint64_t startoff, uint64_t endoff, hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p);
static size_t POSIX_Readat(void * buf, size_t count, uint64_t offset, hpctio_obj_id_t * obj,  hpctio_obj_opt_t * opt, hpctio_sys_params_t * p);
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
  .remove = NULL, // daos remove can force to remove all, hard to mimic the same behavior in posix, and no need for now
  .access = POSIX_Access,
  .rename = POSIX_Rename,
  .stat = POSIX_Stat,
  .obj_options = POSIX_Obj_Options,
  .open = POSIX_Open,
  .close = POSIX_Close,
  .append = POSIX_Append,
  .writeat = POSIX_Writeat,
  .prefetch = POSIX_Prefetch,
  .readat = POSIX_Readat,
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

  char * rdbuf;
  uint64_t rdbuf_start;
  uint64_t rdbuf_end;
  int rdbuf_active;
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

#define CHECK_NOMSG(r, ...)                           \
{                                                    \
    if(r != 0) {                                     \
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
  opt->rdbuf_active = 0;
  opt->rdbuf = NULL;
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
  if(r){
    if(errno == EEXIST){
      CHECK_NOMSG(r);
    }else{
      CHECK(r, "POSIX_Open failed to open the file descriptor for file %s with errno %d", path, errno);
    }
  }
  
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
  hpctio_posix_obj_opt_t * popt = (hpctio_posix_obj_opt_t *)opt;
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
  if(popt->rdbuf){
    free(popt->rdbuf);
    popt->rdbuf = NULL;
    popt->rdbuf_active = 0;
  }

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


static size_t POSIX_Writeat(const void * buf, size_t count, uint64_t offset, hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p){
  hpctio_posix_obj_opt_t * popt = (hpctio_posix_obj_opt_t *) opt;
  CHECK((popt->wmode != HPCTIO_WRITE_AT),  "POSIX - Writing_at to a file without HPCTIO_WRITE_AT mode is not allowed");

  hpctio_posix_fd_t * pfd = (hpctio_posix_fd_t *) obj;
  size_t s = pwrite(pfd->fd, buf, count, offset);

exit:
  if(popt->wmode != HPCTIO_WRITE_AT){
    return -1;
  }else{
    return s == count ? s : -1;
  }
}

static int POSIX_Prefetch(uint64_t startoff, uint64_t endoff, hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p){
    hpctio_posix_obj_opt_t * p_opt = (hpctio_posix_obj_opt_t *) opt;
    
    size_t sz = endoff - startoff;
    if(p_opt->rdbuf){
        p_opt->rdbuf = (char *)realloc(p_opt->rdbuf, sz);
    }else{
        p_opt->rdbuf = (char *)malloc(sz);
    }

    size_t r = 0;
    if(p_opt->rdbuf) r = POSIX_Readat(p_opt->rdbuf, sz, startoff, obj, opt, p);
    if(r == sz) {
        p_opt->rdbuf_active = 1;
        p_opt->rdbuf_start = startoff;
        p_opt->rdbuf_end = endoff;
        return 0;
    }else{
        p_opt->rdbuf_active = 0;
        return -1;
    }

}


static size_t POSIX_Readat(void * buf, size_t count, uint64_t offset, hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p){
  hpctio_posix_fd_t * pfd = (hpctio_posix_fd_t *) obj;
  hpctio_posix_obj_opt_t * p_opt = (hpctio_posix_obj_opt_t *) opt;

  if(p_opt->rdbuf_active && p_opt->rdbuf_start <= offset && (offset + count) <= p_opt->rdbuf_end){
      uint64_t newoff = offset - p_opt->rdbuf_start;
      memcpy(buf, (const char *)(p_opt->rdbuf+newoff), count);
      return count;
  }

  size_t s = pread(pfd->fd, buf, count, offset);
  return s == count ? s : -1;
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


