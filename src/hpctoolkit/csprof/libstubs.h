#ifndef LIBSTUBS_H
#define LIBSTUBS_H

/* We need to get handles on "original" function pointers for several
   functions.  This file commonizes the necessary frobbing. */

/* grab the function pointer and die if we can't find it */
#define CSPROF_GRAB_FUNCPTR(our_name, platform_name) \
do { \
    csprof_ ## our_name = dlsym(RTLD_NEXT, #platform_name); \
    if(csprof_ ## our_name == NULL) { \
        printf("Error in locating " #platform_name "\n"); \
        exit(0); \
    } \
} while(0)

/* we assume that we have a function `init_library_stubs' and a
   `library_stubs_initialized' variable here.  This macro should
   be placed at the entry of any function we override. */
#define MAYBE_INIT_STUBS() do { if(!library_stubs_initialized) { init_library_stubs(); } } while(0)

#endif
