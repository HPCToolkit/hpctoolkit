#include <dlfcn.h>  // dlopen
#include <limits.h>  // PATH_MAX

#include <monitor.h>
#include <lib/prof-lean/pfq-rwlock.h>
#include <hpcrun/loadmap.h>

int NUM_CL_FNS = 4;
static const char *OPENCL_FNS[] = {"clCreateCommandQueue", "clEnqueueNDRangeKernel", "clEnqueueWriteBuffer", "clEnqueueReadBuffer"};
bool isOpenclModuleLoaded = false;
static pfq_rwlock_t modules_lock;

static bool lm_contains_fn (const char *, const char *); 

/* input address start and end of a module entering application space.
if libOpenCL.so has not already been loaded, we checks if current module is libOpenCL.so */

bool checkOpenclModuleLoaded (void *start, void *end)
{
	if (isOpenclModuleLoaded)
		return true;

	// Update path
	// Only one thread could update the flag,
	// Guarantee dlopen modules before notification are updated.
	size_t i;
	bool result = false;
	pfq_rwlock_node_t me;
	pfq_rwlock_write_lock(&modules_lock, &me);

	load_module_t *module = hpcrun_loadmap_findByAddr(start, end);

	if (!pseudo_module_p(module->name))
	{
	   	// this is a real load module; let's see if it contains any of the opencl functions
    	for (i = 0; i < NUM_CL_FNS; ++i) 
		{
			if (lm_contains_fn(module->name, OPENCL_FNS[i])) 
			{
				isOpenclModuleLoaded = true;
				// call constructor of gotcha-opencl-wrapperc
				break;
			}
		}
	}
	pfq_rwlock_write_unlock(&modules_lock, &me);
	return result;
}

static bool lm_contains_fn (const char *lm, const char *fn) 
{
	char resolved_path[PATH_MAX];
	bool load_handle = false;

	char *lm_real = realpath(lm, resolved_path);
	void *handle = monitor_real_dlopen(lm_real, RTLD_NOLOAD);
	// At the start of a program, libs are not loaded by dlopen
	if (handle == NULL)
	{
    	handle = monitor_real_dlopen(lm_real, RTLD_LAZY);
	    load_handle = handle ? true : false;
	}
	PRINT("query path = %s\n", lm_real);
	PRINT("query fn = %s\n", fn);
	PRINT("handle = %p\n", handle);
	bool result = false;
	if (handle && lm_real)
	{
    	void *fp = dlsym(handle, fn);
    	PRINT("fp = %p\n", fp);
		if (fp) 
		{
		    Dl_info dlinfo;
    		int res = dladdr(fp, &dlinfo);
	    	if (res)
			{
    		    char dli_fname_buf[PATH_MAX];
    		    char *dli_fname = realpath(dlinfo.dli_fname, dli_fname_buf);
    		    result = (strcmp(lm_real, dli_fname) == 0);
   		    	PRINT("original path = %s\n", dlinfo.dli_fname);
	    	    PRINT("found path = %s\n", dli_fname);
       		 	PRINT("symbol = %s\n", dlinfo.dli_sname);
      		}
    	}
	}
	// Close the handle it is opened here
	if (load_handle)
    	monitor_real_dlclose(handle);
	return result;
}
