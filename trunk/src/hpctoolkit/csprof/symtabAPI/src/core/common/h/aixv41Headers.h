/*
 * Copyright (c) 1996-2007 Barton P. Miller
 * 
 * We provide the Paradyn Parallel Performance Tools (below
 * described as "Paradyn") on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 * 
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined(_aix_headers_h)
#define _aix_headers_h

typedef int crid_t;
typedef unsigned int class_id_t;

#if USE_PTHREADS
#include <pthread.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */
#include <sys/types.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <memory.h>
#include <netinet/in.h>
#include <netdb.h>
  //#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/reg.h>
#include <sys/ptrace.h>
#include <sys/ldr.h>
#include <sys/resource.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <sys/select.h>
#include <procinfo.h>
#include <sys/un.h>

extern int fork();
extern int vfork();

  typedef int (*P_xdrproc_t)(XDR*, ...);

#if defined(__cplusplus)
};
#endif /* defined(__cplusplus) */

extern char *sys_errlist[];

/* POSIX */
extern int P_getopt(int argc, char *argv[], const char *optstring);
extern void P_abort (void);
extern int P_close (int FILEDES);
extern int P_dup2 (int OLD, int NEW);
extern int P_execvp (const char *FILENAME, char *const ARGV[]);
extern int P_execve (const char* FILENAME,
		     char* const ARGV[], char* const ENVP[]);
extern void P__exit (int STATUS);
extern int P_fcntl (int FILEDES, int command, int arg2);
extern FILE * P_fdopen (int FILEDES, const char *OPENTYPE);
extern FILE * P_fopen (const char *FILENAME, const char *OPENTYPE);
extern int P_fstat (int FILEDES, struct stat *BUF);
extern pid_t P_getpid (void);
extern int P_kill(pid_t PID, int SIGNUM);
extern off_t P_lseek (int FILEDES, off_t OFFSET, int WHENCE);
extern int P_pclose (FILE *STREAM);
extern FILE * P_popen (const char *COMMAND, const char *MODE);
extern int P_open(const char *FILENAME, int FLAGS, mode_t MODE);
extern size_t P_read (int FILEDES, void *BUFFER, size_t SIZE);
extern int P_uname(struct utsname *unm);
extern pid_t P_wait(int *status_ptr);
extern int P_waitpid(pid_t pid, int *statusp, int options);
extern size_t P_write (int FILEDES, const void *BUFFER, size_t SIZE);
inline int P_chdir(const char *path) {return (chdir(path)); }
inline int P_putenv(char *str) {return (putenv(str)); }

/* SYSTEM-V shared memory */
#include <sys/ipc.h>
#include <sys/shm.h> /* shmid_ds */
extern int P_shmget(key_t, int, int);
extern void *P_shmat(int, void *, int);
extern int P_shmdt(void *);
extern int P_shmctl(int, int, struct shmid_ds *);

/* ANSI */
extern void P_exit (int STATUS);
extern int P_fflush(FILE *stream);
extern char * P_fgets (char *S, int P_COUNT, FILE *STREAM);
extern void * P_malloc (size_t SIZE);
extern void * P_memcpy (void *A1, const void *A2, size_t SIZE);
extern void * P_memset (void *BLOCK, int P_C, unsigned SIZE);
extern void P_perror (const char *MESSAGE);
typedef void (*P_sig_handler)(int);
extern P_sig_handler P_signal(int SIGNUM, P_sig_handler ACTION);
extern char * P_strcat (char *TO, const char *FROM);
extern char * P_strchr (const char *P_STRING, int P_C);
extern int P_strcmp (const char *S1, const char *S2);
extern char * P_strcpy (char *TO, const char *FROM);
extern char *P_strdup(const char *S);
extern size_t P_strlen (const char *S);
extern char * P_strncat (char *TO, const char *FROM, size_t SIZE);
extern int P_strncmp (const char *S1, const char *S2, size_t SIZE);
extern char * P_strncpy (char *TO, const char *FROM, size_t SIZE);
extern char * P_strrchr (const char *P_STRING, int P_C);
extern char * P_strstr (const char *HAYSTACK, const char *NEEDLE);
extern double P_strtod (const char *P_STRING, char **TAILPTR);
extern char * P_strtok (char *NEWP_STRING, const char *DELIMITERS);
extern long int P_strtol (const char *P_STRING, char **TAILPTR, int BASE);
extern unsigned long int P_strtoul(const char *P_STRING, char **TAILPTR, int BASE);

/* BSD */
extern int P_accept (int SOCK, struct sockaddr *ADDR, size_t *LENGTH_PTR);
extern int P_bind(int socket, struct sockaddr *addr, size_t len);
extern int P_connect(int socket, struct sockaddr *addr, size_t len);
extern struct hostent * P_gethostbyname (const char *NAME);
/* extern int P_gethostname(char *name, size_t size); */
/* extern int P_getrusage(int, struct rusage*); */
extern struct servent * P_getservbyname (const char *NAME, const char *PROTO);
extern int P_getsockname (int SOCKET, struct sockaddr *ADDR, size_t *LENGTH_PTR);
/* extern int P_gettimeofday (struct timeval *TP, struct timezone *TZP); */
extern int P_listen (int socket, unsigned int n);
caddr_t P_mmap(caddr_t addr, size_t len, int prot, int flags, int fd, off_t off);
extern int P_munmap(caddr_t, int);
extern int P_socket (int NAMESPACE, int STYLE, int PROTOCOL);
extern int P_socketpair(int namesp, int style, int protocol, int filedes[2]);
extern int P_pipe(int fd[2]);
extern int P_strcasecmp(const char *s1, const char *s2);
extern int P_strncasecmp (const char *S1, const char *S2, size_t N);
extern void P_endservent(void);

#ifdef NOTDEF // PDSEP
inline int P_ptrace(int req, int pid, void *addr, int data, void *addr2)
  { return(ptrace(req, pid, (int *)addr, data, (int *)addr2));}
#else
inline int P_ptrace(int req, pid_t pid,  Address addr, Address data, Address addr2)
  { return(ptrace(req, pid, (int *)addr, data, (int *)addr2));}
#endif

extern int P_rexec(char **ahost, u_short inport, char *user, char *passwd, char *cmd, int *fd2p);

extern int P_select(int wid, fd_set *rd, fd_set *wr, fd_set *ex, struct timeval *tm);

extern int P_recv(int s, void *buf, size_t len, int flags);

extern char * P_cplus_demangle( const char * symbol, bool nativeCompiler, bool includeTypes = false );

extern void   P_xdr_destroy(XDR*);
extern bool_t P_xdr_u_char(XDR*, u_char*);
extern bool_t P_xdr_int(XDR*, int*);
extern bool_t P_xdr_double(XDR*, double*);
extern bool_t P_xdr_u_int(XDR*, u_int*);
extern bool_t P_xdr_float(XDR*, float*);
extern bool_t P_xdr_char(XDR*, char*);
extern bool_t P_xdr_string(XDR*, char **, const u_int maxsize);
extern void P_xdrrec_create(XDR*, const u_int send_sz, const u_int rec_sz,
			    const caddr_t handle, 
			    xdr_rd_func read_r, xdr_wr_func write_f);
extern bool_t P_xdrrec_endofrecord(XDR*, int now);
extern bool_t P_xdrrec_skiprecord(XDR*);
inline bool_t P_xdrrec_eof(XDR *x) { return (xdrrec_eof(x)); }


#endif
