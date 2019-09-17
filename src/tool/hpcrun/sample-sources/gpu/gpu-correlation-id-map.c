//
// Created by aleksa on 8/23/19.
//


#include <assert.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "gpu-correlation-id-map.h"

/******************************************************************************
 * type definitions 
 *****************************************************************************/

struct gpu_correlation_id_map_entry_s {
    uint32_t correlation_id;
    uint64_t external_id;
    uint32_t device_id;
    uint64_t start;
    uint64_t end;
    struct gpu_correlation_id_map_entry_s *left;
    struct gpu_correlation_id_map_entry_s *right;
};

/******************************************************************************
 * global data 
 *****************************************************************************/

static gpu_correlation_id_map_entry_t *gpu_correlation_id_map_root = NULL;

/******************************************************************************
 * private operations
 *****************************************************************************/

static gpu_correlation_id_map_entry_t *
gpu_correlation_id_map_entry_new(uint32_t correlation_id, uint64_t external_id)
{
    gpu_correlation_id_map_entry_t *e;
    e = (gpu_correlation_id_map_entry_t *)hpcrun_malloc_safe(sizeof(gpu_correlation_id_map_entry_t));
    e->correlation_id = correlation_id;
    e->external_id = external_id;
    e->left = NULL;
    e->right = NULL;

    return e;
}


static gpu_correlation_id_map_entry_t *
gpu_correlation_id_map_splay(gpu_correlation_id_map_entry_t *root, uint32_t key)
{
    REGULAR_SPLAY_TREE(gpu_correlation_id_map_entry_s, root, key, correlation_id, left, right);
    return root;
}


static void
gpu_correlation_id_map_delete_root()
{
    TMSG(DEFER_CTXT, "correlation_id %d: delete", gpu_correlation_id_map_root->correlation_id);

    if (gpu_correlation_id_map_root->left == NULL) {
        gpu_correlation_id_map_root = gpu_correlation_id_map_root->right;
    } else {
        gpu_correlation_id_map_root->left =
                gpu_correlation_id_map_splay(gpu_correlation_id_map_root->left,
                                               gpu_correlation_id_map_root->correlation_id);
        gpu_correlation_id_map_root->left->right = gpu_correlation_id_map_root->right;
        gpu_correlation_id_map_root = gpu_correlation_id_map_root->left;
    }
}

/******************************************************************************
 * interface operations
 *****************************************************************************/

gpu_correlation_id_map_entry_t *
gpu_correlation_id_map_lookup(uint32_t id)
{
    gpu_correlation_id_map_entry_t *result = NULL;

    gpu_correlation_id_map_root = gpu_correlation_id_map_splay(gpu_correlation_id_map_root, id);
    if (gpu_correlation_id_map_root && gpu_correlation_id_map_root->correlation_id == id) {
        result = gpu_correlation_id_map_root;
    }

    TMSG(DEFER_CTXT, "correlation_id map lookup: id=0x%lx (record %p)", id, result);
    return result;
}


void
gpu_correlation_id_map_insert(uint32_t correlation_id, uint64_t external_id)
{
    gpu_correlation_id_map_entry_t *entry = gpu_correlation_id_map_entry_new(correlation_id, external_id);

    TMSG(DEFER_CTXT, "correlation_id map insert: id=0x%lx (record %p)", correlation_id, entry);

    entry->left = entry->right = NULL;

    if (gpu_correlation_id_map_root != NULL) {
        gpu_correlation_id_map_root =
                gpu_correlation_id_map_splay(gpu_correlation_id_map_root, correlation_id);

        if (correlation_id < gpu_correlation_id_map_root->correlation_id) {
            entry->left = gpu_correlation_id_map_root->left;
            entry->right = gpu_correlation_id_map_root;
            gpu_correlation_id_map_root->left = NULL;
        } else if (correlation_id > gpu_correlation_id_map_root->correlation_id) {
            entry->left = gpu_correlation_id_map_root;
            entry->right = gpu_correlation_id_map_root->right;
            gpu_correlation_id_map_root->right = NULL;
        } else {
            // correlation_id already present: fatal error since a correlation_id 
            //   should only be inserted once 
            assert(0);
        }
    }
    gpu_correlation_id_map_root = entry;
}


// TODO(Keren): remove
void
gpu_correlation_id_map_external_id_replace(uint32_t correlation_id, uint64_t external_id)
{
    TMSG(DEFER_CTXT, "correlation_id map replace: id=0x%lx");

    gpu_correlation_id_map_root =
            gpu_correlation_id_map_splay(gpu_correlation_id_map_root, correlation_id);

    if (gpu_correlation_id_map_root &&
        gpu_correlation_id_map_root->correlation_id == correlation_id) {
        gpu_correlation_id_map_root->external_id = external_id;
    }
}


void
gpu_correlation_id_map_delete(uint32_t correlation_id)
{
    gpu_correlation_id_map_root =
            gpu_correlation_id_map_splay(gpu_correlation_id_map_root, correlation_id);

    if (gpu_correlation_id_map_root &&
        gpu_correlation_id_map_root->correlation_id == correlation_id) {
        gpu_correlation_id_map_delete_root();
    }
}


void
gpu_correlation_id_map_kernel_update
        (
                uint32_t correlation_id,
                uint32_t device_id,
                uint64_t start,
                uint64_t end
        )
{
    TMSG(DEFER_CTXT, "correlation_id map replace: id=0x%lx");

    gpu_correlation_id_map_root =
            gpu_correlation_id_map_splay(gpu_correlation_id_map_root, correlation_id);

    if (gpu_correlation_id_map_root &&
        gpu_correlation_id_map_root->correlation_id == correlation_id) {
        gpu_correlation_id_map_root->device_id = device_id;
        gpu_correlation_id_map_root->start = start;
        gpu_correlation_id_map_root->end = end;
    }
}


uint64_t
gpu_correlation_id_map_entry_external_id_get(gpu_correlation_id_map_entry_t *entry)
{
    return entry->external_id;
}


uint64_t
gpu_correlation_id_map_entry_start_get(gpu_correlation_id_map_entry_t *entry)
{
    return entry->start;
}


uint64_t
gpu_correlation_id_map_entry_end_get(gpu_correlation_id_map_entry_t *entry)
{
    return entry->end;
}


uint32_t
gpu_correlation_id_map_entry_device_id_get(gpu_correlation_id_map_entry_t *entry)
{
    return entry->device_id;
}



/******************************************************************************
 * debugging code
 *****************************************************************************/

static int
gpu_correlation_id_map_count_helper(gpu_correlation_id_map_entry_t *entry)
{
    if (entry) {
        int left = gpu_correlation_id_map_count_helper(entry->left);
        int right = gpu_correlation_id_map_count_helper(entry->right);
        return 1 + right + left;
    }
    return 0;
}


int
gpu_correlation_id_map_count()
{
    return gpu_correlation_id_map_count_helper(gpu_correlation_id_map_root);
}


