#include <stdio.h>
#include <assert.h>
#include <libgen.h>
#include <daos.h>
#include <daos_fs.h>
#include <daos_obj_class.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include "hpctio.h"
#include "hpctio_obj.h"

static int daos_init_count;
static pthread_once_t real_daos_inited;

/************************** PROTOTYPES *****************************/
static hpctio_sys_params_t * DFS_Construct_Params(const char * path);
static int DFS_Compare_Params(const char * path, hpctio_sys_params_t * p);
static void DFS_Display_Params(hpctio_sys_params_t * p);
static char * DFS_Cut_Prefix(const char * path);
static char * DFS_Realpath(const char * path, char * resolved_path);
static void DFS_Init(hpctio_sys_params_t * params);
static void DFS_Final(hpctio_sys_params_t * params);
static int DFS_Mkdir(const char *path, mode_t md, hpctio_sys_params_t * p);
static int DFS_Access(const char *path, int md, hpctio_sys_params_t * p);
static int DFS_Rename(const char *oldpath, const char *newpath, hpctio_sys_params_t * p);

static hpctio_obj_opt_t * DFS_Obj_Options(int wmode, int sizetype);
static hpctio_obj_id_t * DFS_Open(const char * path, int flags, mode_t md, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p);
static int DFS_Close(hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p);


static size_t DFS_Append(const void * buf, size_t size, size_t nitems, hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p);
static long int DFS_Tell(hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt);

/*************************** FILE SYSTEM STRUCTS ***************************/
//file system parameters struct
typedef struct hpctio_dfs_params {
  dfs_t *dfs;
  daos_handle_t poh;
  daos_handle_t coh;

  char* pool;
  char* cont;
  bool inited;
} hpctio_dfs_params_t;

//file system functions struct
hpctio_sys_func_t hpctio_sys_func_dfs = {
    .construct_params = DFS_Construct_Params,
    .compare_params = DFS_Compare_Params,
    .display_params = DFS_Display_Params,
    .cut_prefix = DFS_Cut_Prefix,
    .real_path = DFS_Realpath,
    .initialize = DFS_Init,
    .finalize   = DFS_Final,
    .mkdir = DFS_Mkdir,
    .access = DFS_Access,
    .rename = DFS_Rename,
    .obj_options = DFS_Obj_Options,
    .open = DFS_Open,
    .close = DFS_Close,
    .append = DFS_Append,
    .tell = DFS_Tell
    



};

/*************************** FILE SYSTEM OBJECT STRUCTS ***************************/
//file system objects options struct
typedef struct hpctio_dfs_obj_opt {
  daos_oclass_id_t oc;

  int wmode;
  long file_size;
  int buffer_size;
  int buffer_capacity;
  char * buffer;
}hpctio_dfs_obj_opt_t;

//use dfs_obj_t directly as dfs version of hpctio_obj_id_t 

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

/*
#define CHECK_MPI(MPI_STAT, msg, ...)                                 \
{                                                                     \
    char errstr[MPI_MAX_ERROR_STRING];                                \
    int errlen;                                                       \
    if(MPI_STAT != MPI_SUCCESS) {                                     \
        MPI_Error_string(MPI_STAT, errstr, &errlen);               \
        fprintf(stderr, "ERROR (%s:%d) (MPI %s) : "msg"\n", \
             __FILE__, __LINE__, errstr, ##__VA_ARGS__);        \
        fflush(stderr);                                               \
        MPI_Abort(MPI_COMM_WORLD, -1);                                \
    }                                                                 \
}  
*/

static void actual_daos_init(){
    int r = daos_init();
    CHECK(r, "Failed to initialize daos");
exit: ;
}

/*
* call daos_init() only once if we create multiple DAOS system objects
* reset real_daos_inited after all objects get finalized
*/
static void hpctio_daos_init(){
    assert(daos_init_count >= 0 && "daos_init_count should be non-negative\n");
    // make sure daos_init() can still be called after daos_fini()
    if(daos_init_count == 0){
        real_daos_inited = PTHREAD_ONCE_INIT;
    }
    int r = pthread_once(&real_daos_inited, actual_daos_init);
    CHECK(r, "Failed to initialize daos via pthread_once");
    daos_init_count++;
exit: ;
}

/*
* call daos_fini() when no DAOS system objects left
*/
static void hpctio_daos_fini(){
    if(daos_init_count == 1){
        int r = daos_fini();
        CHECK(r, "Failed to finalize daos with error code %d", r);
    }
    daos_init_count--;

    assert(daos_init_count >= 0 && "hpctio_daos_fini has been called more than hpctio_daos_init\n");
exit: ;
}


// Initialization of Daos without MPI, so no communication between processes
static void dfs_init_regular(hpctio_sys_params_t * params){
    hpctio_dfs_params_t *p = (hpctio_dfs_params_t *) params;
    int r;
    bool daos_inited, pool_connected, cont_opened, dfs_mounted;
    daos_pool_info_t pool_info;
    daos_cont_info_t cont_info;

    p->inited = false;

    if(p->pool == NULL || p->cont == NULL){
        return;
    }

    daos_inited = pool_connected = cont_opened = dfs_mounted = false;

    // initialize daos
    hpctio_daos_init();
    daos_inited = true;

    // connect to a daos pool with parsed id
    uuid_t pool_uuid;
    r = uuid_parse(p->pool, pool_uuid); 
    CHECK(r, "Failed to parse 'Pool uuid': %s", p->pool);
    r = daos_pool_connect(pool_uuid, NULL, DAOS_PC_RW, &(p->poh), &pool_info, NULL);
    CHECK(r, "Failed to connect to 'Pool uuid': %s", p->pool);
    pool_connected = true;

    // open/create a daos container with parsed id
    uuid_t co_uuid;
    r = uuid_parse(p->cont, co_uuid); 
    CHECK(r, "Failed to parse 'Container uuid': %s", p->cont);
    r = daos_cont_open(p->poh, co_uuid, DAOS_COO_RW, &(p->coh), &cont_info, NULL);
    if(r == -DER_NONEXIST){
        CHECK(r, "Failed to open container: container does not exist");
    }else{
        CHECK(r, "Failed to open container");
    }
    cont_opened = true;
    
    // mount on daos
    r = dfs_mount(p->poh, p->coh, DFS_RDWR, &(p->dfs));
    CHECK(r, "Failed to mount DFS namespace with pool %s and cont %s", p->pool, p->cont);
    dfs_mounted = true;

    // mark the params to be succesfully initialized
    p->inited = true;


exit:
    if(r){
        if(dfs_mounted) dfs_umount(p->dfs);
        if(cont_opened) daos_cont_close(p->coh, NULL);
        if(pool_connected) daos_pool_disconnect(p->poh, NULL);
        if(daos_inited) hpctio_daos_fini();
    }

}

enum handleType {
    POOL_HANDLE,
    CONT_HANDLE,
	DFS_HANDLE
};

/*
// 0 on success, erron on failure
static int handle_bcast(enum handleType type, hpctio_dfs_params_t *p, MPI_Comm comm, int rank){
    d_iov_t global;
    int r;

    global.iov_buf = NULL;
    global.iov_buf_len = 0;
    global.iov_len = 0;

    // set iov_buf == NULL, so that we can get the global handle size this round
    if(rank == 0){
        // "If glob->iov_buf is set to NULL, the actual size of the global handle is returned through glob->iov_buf_len"
        if(type == POOL_HANDLE) 
            r = daos_pool_local2global(p->poh, &global);
        else if(type == CONT_HANDLE) 
            r = daos_cont_local2global(p->coh, &global);
        else 
            r = dfs_local2global(p->dfs, &global);
        CHECK(r, "Failed to get global handle size.");
    }

    // bcast the handle size and set up
    CHECK_MPI(MPI_Bcast(&global.iov_buf_len, 1, MPI_UINT64_T, 0, comm), "Failed to bcast handle size");

    global.iov_len = global.iov_buf_len;
    global.iov_buf = malloc(global.iov_buf_len);
    CHECK((global.iov_buf == NULL), "Failed to allocate handle buffer, global.");

    // iov_buf gets malloced, so that we can get the actual handle this round
    if(rank == 0){
        if(type == POOL_HANDLE) 
            r = daos_pool_local2global(p->poh, &global);
        else if(type == CONT_HANDLE) 
            r = daos_cont_local2global(p->coh, &global);
        else 
            r = dfs_local2global(p->dfs, &global);
        CHECK(r, "Failed to get global handle.");
    }

    // bcast the actual handle and set up
    CHECK_MPI(MPI_Bcast(global.iov_buf, global.iov_buf_len, MPI_BYTE, 0, comm), "Failed to bcast handle");
    if(rank != 0){
        if(type == POOL_HANDLE) 
            r = daos_pool_global2local(global, &(p->poh));
        else if(type == CONT_HANDLE) 
            r = daos_cont_global2local(p->poh, global, &(p->coh));
        else 
            r = dfs_global2local(p->poh, p->coh, 0, global, &(p->dfs));
        CHECK(r, "Failed to get local handle with handle type = %d", type);
    }

exit:
    if(global.iov_buf) free(global.iov_buf);
    return r;

}


// Initialization of Daos WITH MPI, so yes communication between processes
static void dfs_init_mpi(hpctio_sys_params_t * params, int rank, MPI_Comm comm){
    hpctio_dfs_params_t *p = (hpctio_dfs_params_t *) params;
    int r;
    bool daos_inited, pool_connected, cont_opened, dfs_mounted;

    p->inited = false;

    if(p->pool == NULL || p->cont == NULL){
        return;
    }

    daos_inited = pool_connected = cont_opened = dfs_mounted = false;

    // initialize daos
    hpctio_daos_init();
    daos_inited = true;

    if(rank == 0){
        daos_pool_info_t pool_info;
        daos_cont_info_t cont_info;

        // connect to a daos pool with parsed id
        uuid_t pool_uuid;
        r = uuid_parse(p->pool, pool_uuid); 
        CHECK(r, "Failed to parse 'Pool uuid': %s", p->pool);
        r = daos_pool_connect(pool_uuid, NULL, DAOS_PC_RW, &(p->poh), &pool_info, NULL);
        CHECK(r, "Failed to connect to 'Pool uuid': %s", p->pool);
        pool_connected = true;

        // open/create a daos container with parsed id
        uuid_t co_uuid;
        r = uuid_parse(p->cont, co_uuid); 
        CHECK(r, "Failed to parse 'Container uuid': %s", p->cont);
        r = daos_cont_open(p->poh, co_uuid, DAOS_COO_RW, &(p->coh), &cont_info, NULL);
        if(r == -DER_NONEXIST){
            CHECK(r, "Failed to open container: container does not exist");
        }else{
            CHECK(r, "Failed to open container");
        }
        cont_opened = true;

         // mount on daos
        r = dfs_mount(p->poh, p->coh, DFS_RDWR, &(p->dfs));
        CHECK(r, "Failed to mount DFS namespace with pool %s and cont %s", p->pool, p->cont);
        dfs_mounted = true;

    }

    // bcast the handles to other processes
    handle_bcast(POOL_HANDLE, p, comm, rank);
    pool_connected = true;
    handle_bcast(CONT_HANDLE, p, comm, rank);
    cont_opened = true;
    handle_bcast(DFS_HANDLE, p, comm, rank);
    dfs_mounted = true;

    // mark the params to be succesfully initialized
    p->inited = true;

exit:
    if(r){
        if(dfs_mounted) dfs_umount(p->dfs);
        if(cont_opened) daos_cont_close(p->coh, NULL);
        if(pool_connected) daos_pool_disconnect(p->poh, NULL);
        if(daos_inited) hpctio_daos_fini();
    }


}
*/

// Finalization of DAOS without MPI, so no communication between processes
static void dfs_final_regular(hpctio_sys_params_t * params){
    hpctio_dfs_params_t *p = (hpctio_dfs_params_t *) params;
    int r;

    r = dfs_umount(p->dfs);
    CHECK(r, "Failed to umount DFS namespace with pool %s and cont %s", p->pool, p->cont);

    r = daos_cont_close(p->coh, NULL);
    CHECK(r, "Failed to close container %s (error code: %d)", p->cont, r);

    r = daos_pool_disconnect(p->poh, NULL);
    CHECK(r, "Failed to disconnect from pool %s (error code: %d)", p->pool, r);

    hpctio_daos_fini();

exit:
    p->inited = false;
}

/*
// Finalization of DAOS with MPI, so yes communication between processes
static void dfs_final_mpi(hpctio_sys_params_t * params, MPI_Comm comm){
    hpctio_dfs_params_t *p = (hpctio_dfs_params_t *) params;
    int r;

    MPI_Barrier(comm);

    r = dfs_umount(p->dfs);
    CHECK(r, "Failed to umount DFS namespace with pool %s and cont %s", p->pool, p->cont);
    MPI_Barrier(comm);

    r = daos_cont_close(p->coh, NULL);
    CHECK(r, "Failed to close container %s (error code: %d)", p->cont, r);
    MPI_Barrier(comm);

    r = daos_pool_disconnect(p->poh, NULL);
    CHECK(r, "Failed to disconnect from pool %s (error code: %d)", p->pool, r);
    MPI_Barrier(comm);

    hpctio_daos_fini();
    MPI_Barrier(comm);

exit:
    p->inited = false;
}
*/

// split a path into the object name and the directory
static int parse_filename(const char * path, char ** objn, char ** contn){
    int r = 0;

    if (path == NULL || objn == NULL || contn == NULL)
		return EINVAL;

    char * path1 = strdup(path);
    char * path2 = strdup(path);

    if(path1 == NULL || path2 == NULL){
        r = ENOMEM;
        goto exit;
    }

    char* filen = basename(path1);
    char* dirn = dirname(path2);

    *objn = strdup(filen);
    if (*objn == NULL) {
        r = ENOMEM;
        goto exit;
    }

    
    if(dirn[0] == '/'){
        *contn = strdup(dirn);
    }else{
        char buf[PATH_MAX];
        sprintf(buf, "/%s", dirn);
        *contn = strdup(buf);
    }
    if (*contn == NULL) {
        r = ENOMEM;
        goto exit;
    }

exit:
    if(path1) free(path1);
    if(path2) free(path2);
    return r;

}


// write len of bytes from buf to off at obj
// return number of bytes written on success and -1 on failure with errno set
static int real_write(const void * buf, size_t len, daos_off_t off, dfs_t * dfs, dfs_obj_t * obj){
    //prepare the scatter gather list for writing
    d_iov_t iov; //io vector for memory buffer 
    d_sg_list_t sgl; //scatter and gather list
    sgl.sg_nr = 1;
    sgl.sg_nr_out = 0;
    d_iov_set(&iov, buf, len);
    sgl.sg_iovs = &iov;
    
    int r = dfs_write(dfs, obj, &sgl, off, NULL);
    CHECK(r, "DFS - failed to dfs_write with errno %d", r);

exit:
    if(r){
        errno = r;
        r = -1;
    }else{
        r = len;
    }
    return r;
}


static hpctio_sys_params_t * DFS_Construct_Params(const char * path){
    hpctio_dfs_params_t * sys_p = (hpctio_dfs_params_t *)malloc(sizeof(hpctio_dfs_params_t));
    sys_p->inited = false;

    char * path_copy = strdup(path);
    path_copy += strlen(hpctio_daos_prefix);
    
    sys_p->pool = strtok(path_copy, "/");
    sys_p->cont = strtok(NULL, "/");

    assert(sys_p->pool != NULL && sys_p->cont != NULL);

    return (hpctio_sys_params_t *)sys_p;
}

// return 0 if equal, -1 if not
static int DFS_Compare_Params(const char * path, hpctio_sys_params_t * p){
    char * path_copy = strdup(path);
    path_copy += strlen(hpctio_daos_prefix);   
    char * pool = strtok(path_copy, "/");
    char * cont = strtok(NULL, "/");

    if(strcmp(pool, ((hpctio_dfs_params_t *)p)->pool) == 0 
        && strcmp(cont, ((hpctio_dfs_params_t *)p)->cont) == 0)
            return 0;
    
    return -1;
}

static void DFS_Display_Params(hpctio_sys_params_t * p){
    hpctio_dfs_params_t * params = (hpctio_dfs_params_t *) p;
    printf("Pool: %s\nCont: %s\nInited: %d\n", params->pool, params->cont, params->inited);
}

static char * DFS_Cut_Prefix(const char * path){
    char * path_copy = strdup(path);
    path_copy += strlen(hpctio_daos_prefix);
    
    char * pool = strtok(path_copy, "/");
    char * cont = strtok(NULL, "/");
    path_copy += strlen(pool) + strlen(cont) + 2;

    return path_copy;
}

static char * DFS_Realpath(const char * path, char * resolved_path){
    int r = sprintf(resolved_path, "%s", path);
    return r == strlen(resolved_path) ? resolved_path : NULL;
}


static void DFS_Init(hpctio_sys_params_t * params){
    /*
    int mpi_avail;
    CHECK_MPI(MPI_Initialized(&mpi_avail), "Failed to check if MPI initialized");

    if(mpi_avail){
        //TODO: log printf("DFS init MPI version\n");
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        dfs_init_mpi(params, rank, MPI_COMM_WORLD);
    }else{*/
        //TODO: log printf("DFS init NONMPI version\n");
        dfs_init_regular(params);
    //}
}



static void DFS_Final(hpctio_sys_params_t * params){
    /*
    int mpi_avail;
    CHECK_MPI(MPI_Initialized(&mpi_avail), "Failed to check if MPI initialized");

    if(mpi_avail){
        //TODO: log printf("DFS final MPI version\n");
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        dfs_final_mpi(params, MPI_COMM_WORLD);
    }else{*/
        //TODO: log printf("DFS final NONMPI version\n");
        dfs_final_regular(params);
    //}
}


static int DFS_Mkdir(const char *path, mode_t md, hpctio_sys_params_t * p){    
    hpctio_dfs_params_t * dfs_p = (hpctio_dfs_params_t *) p;

    dfs_obj_t * parent = NULL;
    char * name = NULL;
    char * dir_name = NULL;
    int r;

    r = parse_filename(path, &name, &dir_name);
    CHECK(r, "Failed to parse path name %s with error code %d", path, r);

    if(strcmp(dir_name, "/.") != 0){
        r = dfs_lookup(dfs_p->dfs, dir_name, O_RDWR, &parent, NULL, NULL);
        CHECK(r, "Failed to look up the directory from DFS %s with errno %d", dir_name, r);
    }

    r = dfs_mkdir(dfs_p->dfs, parent, name, md, OC_SX);
    if(r == EEXIST){
        r = 0;
    }

exit:
    if(name) free(name);
    if(dir_name) free(dir_name);
    if(parent) dfs_release(parent);
    if(r){
        errno = r;
        r = -1;  
    }
    return r;
    
}

static int DFS_Access(const char *path, int md, hpctio_sys_params_t * p){
    hpctio_dfs_params_t * dfs_p = (hpctio_dfs_params_t *) p;

    dfs_obj_t * parent = NULL;
    char * name = NULL;
    char * dir_name = NULL;
    int r;

    r = parse_filename(path, &name, &dir_name);
    CHECK(r, "Failed to parse path name %s with error code %d", path, r);

    if(strcmp(dir_name, "/.") != 0){
        r = dfs_lookup(dfs_p->dfs, dir_name, O_RDWR, &parent, NULL, NULL);
        CHECK(r, "Failed to look up the directory from DFS %s with errno %d", dir_name, r);
    }

    r = dfs_access(dfs_p->dfs, parent, name, md);

exit:
    if(name) free(name);
    if(dir_name) free(dir_name);
    if(parent) dfs_release(parent);
    if(r){
        errno = r;
        r = -1;
    }
    return r;
}

static int DFS_Rename(const char *oldpath, const char *newpath, hpctio_sys_params_t * p){
    hpctio_dfs_params_t * dfs_p = (hpctio_dfs_params_t *) p;

    dfs_obj_t * oldparent = NULL;
    dfs_obj_t * newparent = NULL;
    char * oldname = NULL;
    char * olddir_name = NULL;
    char * newname = NULL;
    char * newdir_name = NULL;
    int r;

    r = parse_filename(oldpath, &oldname, &olddir_name);
    CHECK(r, "Failed to parse path name %s with error code %d", oldpath, r);

    r = parse_filename(newpath, &newname, &newdir_name);
    CHECK(r, "Failed to parse path name %s with error code %d", newpath, r);

    if(strcmp(olddir_name, "/.") != 0){
        r = dfs_lookup(dfs_p->dfs, olddir_name, O_RDWR, &oldparent, NULL, NULL);
        CHECK(r, "Failed to look up the directory from DFS %s with errno %d", olddir_name, r);
    }
    if(strcmp(newdir_name, "/.") != 0){
        r = dfs_lookup(dfs_p->dfs, newdir_name, O_RDWR, &newparent, NULL, NULL);
        CHECK(r, "Failed to look up the directory from DFS %s with errno %d", newdir_name, r);
    }

    r = dfs_move(dfs_p->dfs, oldparent, oldname, newparent, newname, NULL);

exit:
    if(oldname) free(oldname);
    if(newname) free(newname);
    if(olddir_name) free(olddir_name);
    if(newdir_name) free(newdir_name);
    if(oldparent) dfs_release(oldparent);
    if(newparent) dfs_release(newparent);
    if(r){
        errno = r;
        r = -1;
    }
    return r;
}


static hpctio_obj_opt_t * DFS_Obj_Options(int wmode, int sizetype){
    hpctio_dfs_obj_opt_t * opt = (hpctio_dfs_obj_opt_t *) malloc(sizeof(hpctio_dfs_obj_opt_t));
    if(sizetype == HPCTIO_SMALL_F) 
        opt->oc = OC_S1;   
    if(sizetype == HPCTIO_LARGE_F)
        opt->oc = OC_EC_2P1GX; //on Aurora, it should be OC_EC_16P2GX
    
    opt->wmode = wmode;
    opt->buffer_capacity = 1024 * 1024;
    return (hpctio_obj_opt_t *)opt;
}

static hpctio_obj_id_t * DFS_Open(const char * path, int flags, mode_t md, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p){
    hpctio_dfs_params_t * dfs_p = (hpctio_dfs_params_t *) p;
    hpctio_dfs_obj_opt_t * dfs_opt = (hpctio_dfs_obj_opt_t *) opt;

    char * name = NULL;
    char * dir_name = NULL;
    dfs_obj_t * obj = NULL; 
    dfs_obj_t * parent = NULL; 

    int r = parse_filename(path, &name, &dir_name);
    CHECK(r, "Failed to parse path %s with errno %d", path, r);

    if(strcmp(dir_name, "/.") != 0){
        r = dfs_lookup(dfs_p->dfs, dir_name, O_RDWR, &parent, NULL, NULL);
        CHECK(r, "Failed to look up the directory from DFS %s with errno %d", dir_name, r);
    }

    r = dfs_open(dfs_p->dfs, parent, name, md | S_IFREG, flags, dfs_opt->oc, 0, NULL, &obj);

    if(dfs_opt->wmode == HPCTIO_APPEND){
        dfs_opt->file_size = 0;
        dfs_opt->buffer_size = 0;
        dfs_opt->buffer = (char *) malloc(dfs_opt->buffer_capacity); 
    }

exit:
    if(name) free(name);
    if(dir_name) free(dir_name);
    if(parent) dfs_release(parent);
    if(r){
        errno = r;
        obj = NULL;
    }

    return (hpctio_obj_id_t *) obj;
}


static int DFS_Close(hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p){
    dfs_obj_t * dobj = (dfs_obj_t *) obj;
    hpctio_dfs_obj_opt_t * dfs_opt = (hpctio_dfs_obj_opt_t *) opt;
    hpctio_dfs_params_t * dfs_p = (hpctio_dfs_params_t *) p;  
    int r;

    // check if any data remain in the buffer to be flushed
    if((dfs_opt->wmode == HPCTIO_APPEND) && (dfs_opt->buffer_size > 0)){
        r = real_write(dfs_opt->buffer, dfs_opt->buffer_size, dfs_opt->file_size, dfs_p->dfs, dobj);
        if(r == dfs_opt->buffer_size){
            dfs_opt->buffer_size = 0;
            dfs_opt->file_size += r;
        }
        CHECK((r == -1), "Failed to flush the buffer during DFS_Close() the file object");
    }

    if(dfs_opt->wmode == HPCTIO_APPEND){
        free(dfs_opt->buffer);
        dfs_opt->buffer = NULL;
    }

    r = dfs_release(dobj);

exit:
    if(r && (r != -1)){
        errno = r;
        r = -1;
    }
    return r;
}


static size_t DFS_Append(const void * buf, size_t size, size_t nitems, hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt, hpctio_sys_params_t * p){
    hpctio_dfs_obj_opt_t * dfs_opt = (hpctio_dfs_obj_opt_t *) opt;
    CHECK((dfs_opt->wmode != HPCTIO_APPEND),  "DFS - Appending to a file without HPCTIO_APPEND mode is not allowed");

    hpctio_dfs_params_t * dfs_p = (hpctio_dfs_params_t *) p;
    dfs_obj_t * dobj = (dfs_obj_t *) obj;
    int r;

    if((dfs_opt->buffer_size + size*nitems) >= dfs_opt->buffer_capacity){
        r = real_write(dfs_opt->buffer, dfs_opt->buffer_size, dfs_opt->file_size, dfs_p->dfs, dobj);
        if(r == dfs_opt->buffer_size){
            dfs_opt->buffer_size = 0;
            dfs_opt->file_size += r;
        }
        CHECK((r == -1), "Failed to flush the buffer during DFS_Append() the file object");
    }

    if(size*nitems >= dfs_opt->buffer_capacity){
        r = real_write(buf, size*nitems, dfs_opt->file_size, dfs_p->dfs, dobj);
        if(r == size*nitems){
            dfs_opt->file_size += r;
        }
        CHECK((r == -1), "Failed to write the large data directly during DFS_Close() the file object");
    }else{
        memmove(&(dfs_opt->buffer[dfs_opt->buffer_size]), buf, size * nitems);
        dfs_opt->buffer_size += size * nitems;
        r = size * nitems;
    }

exit:
    return r;
}


static long int DFS_Tell(hpctio_obj_id_t * obj, hpctio_obj_opt_t * opt){
    hpctio_dfs_obj_opt_t * dfs_opt = (hpctio_dfs_obj_opt_t *) opt;

    if(dfs_opt->wmode == HPCTIO_APPEND){
        return (dfs_opt->file_size + dfs_opt->buffer_size);
    }else{ //HPCTIO_WRITE_AT or error
        return -1;
    }
    
}


