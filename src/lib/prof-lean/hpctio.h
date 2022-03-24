#ifndef HPCTIO_H
#define HPCTIO_H

#include <mpi.h>
#include <sys/stat.h>

/*************************** FILE SYSTEM OBJECT RELATED STRUCTS ***************************/
//file system objects options struct
typedef struct hpctio_obj_opt {
  void* dummy;
}hpctio_obj_opt_t;

//file system object struct
typedef struct hpctio_obj_id {
  void* dummy;
}hpctio_obj_id_t;


/*************************** FILE SYSTEM STRUCTS ***************************/
//file system parameters struct
typedef struct hpctio_sys_params {
  void* dummy;
} hpctio_sys_params_t;

//file system functions struct
typedef struct hpctio_sys_func {
  hpctio_sys_params_t * (*construct_params)(const char * path);
  int (*compare_params)(const char * path, hpctio_sys_params_t * p);
  void (*display_params)(hpctio_sys_params_t * p);
  char * (*cut_prefix)(const char * path);
  char * (*real_path)(const char * path, char * resolved_path);

  void (*initialize)(hpctio_sys_params_t * params);
  void (*finalize)(hpctio_sys_params_t * params);

  int (*mkdir)(const char *path, mode_t md, hpctio_sys_params_t * p);
  int (*access)(const char *path, int md, hpctio_sys_params_t * p);
  int (*rename)(const char *old_path, const char * new_path, hpctio_sys_params_t * p);

  hpctio_obj_opt_t * (*obj_options)(int writemode);
  hpctio_obj_id_t * (*open)(const char * path, int flags, mode_t md, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p);




  /*
   hpctio_fd_t * (*create)(const char *path, mode_t md);
    int (*delete)(const char * path);
    hpctio_fd_t * (*open)(const char *path, int flags, mode_t md);
    int (*close)(hpctio_fd_t * fd);

    int (*mkdir)(const char *path, mode_t md);
    int (*rmdir)(const char *path);

    int (*write)(hpctio_fd_t * fd, const void* buf, hpctio_size_t size, hpctio_size_t num, hpctio_offset_t off);
    int (*read)(hpctio_fd_t * fd, void* buf, hpctio_size_t size, hpctio_size_t num, hpctio_offset_t off);

    hpctio_mod_opt_t * (*get_options)(int argc, char **argv);
    */



} hpctio_sys_func_t;

extern hpctio_sys_func_t hpctio_sys_func_dfs;
extern hpctio_sys_func_t hpctio_sys_func_posix;

//file system struct
typedef struct hpctio_sys {
  hpctio_sys_func_t* func_ptr;
  hpctio_sys_params_t* params_ptr;  
} hpctio_sys_t;

extern hpctio_sys_t hpctio_sys_posix;


/*************************** TYPE DEFINITIONS ***************************/
typedef long long int hpctio_offset_t;
typedef long long int hpctio_size_t;


/*************************** GLOBAL I/O CONSTANTS ***************************/
extern const char *  hpctio_daos_prefix;


/*************************** FILE SYSTEM FUNCTIONS ***************************/
hpctio_sys_t * hpctio_sys_initialize(const char * path);
hpctio_sys_t * hpctio_sys_default();
void hpctio_sys_finalize(hpctio_sys_t * sys);
void hpctio_sys_avail_display();

char * hpctio_sys_cut_prefix(const char * path, hpctio_sys_t * sys);
char * hpctio_sys_realpath(const char * path, char * resolved_path, hpctio_sys_t * sys);

int hpctio_sys_mkdir(const char *path, mode_t md, hpctio_sys_t * sys);
int hpctio_sys_access(const char *path, int md, hpctio_sys_t * sys);
int hpctio_sys_rename(const char *old_path, const char *new_path, hpctio_sys_t * sys);

#endif /* ifndef HPCTIO_H */