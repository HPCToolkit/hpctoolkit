/*
 * Library that makes calls to dlopen(), dlclose(), pthread_create(),
 * fork() and _exit() from its init constructor.  This causes trouble
 * for hpcrun and monitor.
 *
 * Usage: set environ EARLY to string of chars: 'c' (dlclose),
 * 'e' (_exit), 'f' (fork), 'o' (dlopen) or 't' (pthread_create),
 * and run the emain program.
 *
 * Example:  EARLY=fo ./emain
 *
 * to have the library call fork() and dlopen() from its init
 * constructor.
 *
 * $Id$
 */

#include <sys/types.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TITLE(name)  printf("----------  %s  ----------\n", name);

void *handle = NULL;

void
early_fcn(void)
{
    printf("==> main calls %s\n", __func__);
}

void
do_dlopen(void)
{
    TITLE("dlopen");
    handle = dlopen("/lib64/libm.so.6", RTLD_LAZY);
    printf("dlopen: handle = %p\n", handle);
}

void
do_dlclose(void)
{
    int ret;

    TITLE("dlclose");
    if (handle == NULL) {
	printf("dlclose: handle is NULL\n");
	return;
    }
    ret = dlclose(handle);
    printf("dlclose: ret = %d\n", ret);
}

void
do_fork(void)
{
    pid_t pid;

    TITLE("fork");
    pid = fork();
    printf("fork: pid = %d\n", getpid());
}

void
do_exit(void)
{
    TITLE("_exit");
    _exit(0);
}

void *
mythread(void *data)
{
    int k;

    for (k = 1; k <= 20; k++) {
	printf("new pthread: k = %d\n", k);
	sleep(1);
    }
    return (NULL);
}

void
do_thread(void)
{
    pthread_t td;
    int ret;

    TITLE("pthread");
    ret = pthread_create(&td, NULL, mythread, NULL);
    if (ret == 0)
	printf("pthread: launched new thread\n");
    else
	printf("pthread: new thread FAILED\n");
}

void __attribute__ ((constructor))
libearly_init(void)
{
    char *s;
    int k;

    s = getenv("EARLY");
    if (s == NULL) {
	printf("%s: EARLY = %s\n", __func__, "(not set)");
	return;
    }
    printf("%s: EARLY = %s\n", __func__, s);

    for (k = 0; s[k] != 0; k++) {
	switch (s[k]) {
	case 'C': case 'c':
	    do_dlclose();
	    break;

	case 'E': case 'e':
	    do_exit();
	    break;

	case 'F': case 'f':
	    do_fork();
	    break;

	case 'O': case 'o':
	    do_dlopen();
	    break;

	case 'T': case 't':
	    do_thread();
	    break;

	default:
	    printf("%s: unknown char: %c\n", __func__, s[k]);
	    break;
	}
    }
}
