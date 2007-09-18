#include <loader.h>
#include <string.h>

#include "general.h"
#include "interface.h"
#include "mem.h"

static char *
csprof_epoch_search(csprof_epoch_t *epoch, char *modname)
{
    csprof_epoch_module_t *runner;

    for(runner = epoch->loaded_modules;
        runner != NULL;
        runner = runner->next) {
        char *candidate = runner->module_name;

        if(strcmp(candidate, modname) == 0) {
            return candidate;
        }
    }

    /* not found in the previous epoch */
    return NULL;
}

static void
csprof_epoch_dump_module_info(ldr_process_t me,
                              ldr_module_t moduleid,
                              ldr_module_info_t *moduleinfo)
{
    ldr_region_info_t regioninfo;
    int region, status;
    size_t ret_region_size;

    /* ugh, FIXME */
    IFMSG(1) {
        MSG(1, "shared object %s loaded", moduleinfo->lmi_name);

        for(region=0; region<moduleinfo->lmi_nregion; ++region) {
            status = ldr_inq_region(me, moduleid, region,
                                    &regioninfo, sizeof(ldr_region_info_t),
                                    &ret_region_size);

            if(status != 0) {
                ERRMSG("ldr_inq_region failed!\n", __FILE__, __LINE__);
                continue;
            }

            MSG(1, "  region %s: vaddr: %lx / mapaddr: %lx",
                regioninfo.lri_name, regioninfo.lri_vaddr,
                regioninfo.lri_mapaddr);
        }
    }
}

/* we overload the meaning of "module" in this function.  Digital treats
   "module" as "shared object"; each module has "regions" (text, data, etc.).
   we don't really care about any region but the text region, so we just
   refer to that region as the "module". */
static void
csprof_epoch_find_text_region(ldr_process_t me, ldr_module_t moduleid,
                              ldr_module_info_t *moduleinfo,
                              csprof_epoch_module_t *text_region)
{
    ldr_region_info_t regioninfo;
    int region, status;
    size_t ret_region_size;

    for(region=0; region<moduleinfo->lmi_nregion; ++region) {
        status = ldr_inq_region(me, moduleid, region,
                                &regioninfo, sizeof(ldr_region_info_t),
                                &ret_region_size);

        if(status != 0) {
            ERRMSG("ldr_inq_region failed!  Cannot locate .text region\n",
                   __FILE__, __LINE__);
            goto error;
        }

        if(strcmp(regioninfo.lri_name, ".text") == 0) {
            text_region->vaddr = regioninfo.lri_vaddr;
            text_region->mapaddr = regioninfo.lri_mapaddr;
            return;
        }
    }

    /* not reached */
 error:
    text_region->vaddr = NULL;
    text_region->mapaddr = NULL;
}

void
csprof_epoch_get_loaded_modules(csprof_epoch_t *epoch,
                                csprof_epoch_t *previous_epoch)
{
    /* code cribbed from the Boehm garbage collector.  we'd almost
       rather do things through ioctl() and /proc, since that's
       slightly more portable, but /proc on Tru64 doesn't give us
       information about the filenames of the modules we've loaded,
       only the address ranges, which is not very helpful */
    int status;
    ldr_process_t me;

    /* information about modules */
    ldr_module_t moduleid = LDR_NULL_MODULE;
    ldr_module_info_t moduleinfo;
    size_t moduleinfo_size = sizeof(moduleinfo);
    size_t ret_module_size;

    me = ldr_my_process();

    while(TRUE) {
        char *modname = NULL;

        status = ldr_next_module(me, &moduleid);

        if(moduleid == LDR_NULL_MODULE) {
            break;
        }

        /* Boehm says this is to handle a bug and actually returns
           an error in this case; I'm not quite sure what to do... */
        if(status != 0) {
            ERRMSG("ldr_next_module failed!\n"
                   "    profiling information will be incomplete",
                   __FILE__, __LINE__);
            return;
        }

        status = ldr_inq_module(me, moduleid, &moduleinfo,
                                moduleinfo_size, &ret_module_size);

        if(status != 0) {
            ERRMSG("ldr_inq_module failed!\n"
                   "    profiling information will be incomplete",
                   __FILE__, __LINE__);
            return;
        }

        if((previous_epoch == NULL)
           || !(modname = csprof_epoch_search(previous_epoch,
                                              moduleinfo.lmi_name))) {
            /* dump some information about the module */
            csprof_epoch_dump_module_info(me, moduleid, &moduleinfo);

            /* haven't seen that name before, make a copy */
            modname = csprof_malloc(strlen(moduleinfo.lmi_name)+1);

            strcpy(modname, moduleinfo.lmi_name);
        }

        /* fill in needed data */
        {
            csprof_epoch_module_t *new_module
                = csprof_malloc(sizeof(csprof_epoch_module_t));

            new_module->module_name = modname;
            csprof_epoch_find_text_region(me, moduleid,
                                          &moduleinfo, new_module);
            
            /* push the module into the epoch */
            new_module->next = epoch->loaded_modules;
            epoch->loaded_modules = new_module;

            /* a new module! */
            epoch->num_modules++;
        }
    }
}
