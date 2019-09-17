//
// Created by user on 19.8.2019..
//

#include "gpu-host-op-map.h"
#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#define GPU_HOST_OP_MAP_DEBUG 0

#if GPU_HOST_OP_MAP_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

static gpu_host_op_map_entry_t *gpu_host_op_map_root = NULL;

static gpu_host_op_map_entry_t *
gpu_host_op_map_entry_new(uint64_t host_op_id, cct_node_t *host_op_node, gpu_record_t *record, entry_data_t *data)
{
    gpu_host_op_map_entry_t *e;
    e = (gpu_host_op_map_entry_t *)hpcrun_malloc_safe(sizeof(gpu_host_op_map_entry_t));
    e->host_op_id = host_op_id;
    e->data = data;
    e->samples = 0;
    e->total_samples = 0;
    e->host_op_node = host_op_node;
    e->record = record;
    e->left = NULL;
    e->right = NULL;

    return e;
}

static gpu_host_op_map_entry_t *
gpu_host_op_map_splay(gpu_host_op_map_entry_t *root, uint64_t key)
{
    REGULAR_SPLAY_TREE(gpu_host_op_map_entry_s, root, key, host_op_id, left, right);
    return root;
}

static void
gpu_host_op_map_delete_root()
{
    TMSG(DEFER_CTXT, "host op %d: delete", gpu_host_op_map_root->host_op_id);

    if (gpu_host_op_map_root->left == NULL) {
        gpu_host_op_map_root = gpu_host_op_map_root->right;
    } else {
        gpu_host_op_map_root->left =
                gpu_host_op_map_splay(gpu_host_op_map_root->left,
                                        gpu_host_op_map_root->host_op_id);
        gpu_host_op_map_root->left->right = gpu_host_op_map_root->right;
        gpu_host_op_map_root = gpu_host_op_map_root->left;
    }
}

gpu_host_op_map_entry_t *
gpu_host_op_map_lookup(uint64_t id)
{
    gpu_host_op_map_entry_t *result = NULL;

    gpu_host_op_map_root = gpu_host_op_map_splay(gpu_host_op_map_root, id);
    if (gpu_host_op_map_root && gpu_host_op_map_root->host_op_id == id) {
        result = gpu_host_op_map_root;
    }

    TMSG(DEFER_CTXT, "host op map lookup: id=0x%lx (record %p)", id, result);
    return result;
}


void
gpu_host_op_map_insert(uint64_t host_op_id, cct_node_t *host_op_node, gpu_record_t *record, entry_data_t *data)
{
    gpu_host_op_map_entry_t *entry = gpu_host_op_map_entry_new(host_op_id, host_op_node, record, data);

    TMSG(DEFER_CTXT, "host op map insert: id=0x%lx seq_id=0x%lx", host_op_id, host_op_node);

    entry->left = entry->right = NULL;

    if (gpu_host_op_map_root != NULL) {
        gpu_host_op_map_root =
                gpu_host_op_map_splay(gpu_host_op_map_root, host_op_id);

        if (host_op_id < gpu_host_op_map_root->host_op_id) {
            entry->left = gpu_host_op_map_root->left;
            entry->right = gpu_host_op_map_root;
            gpu_host_op_map_root->left = NULL;
        } else if (host_op_id > gpu_host_op_map_root->host_op_id) {
            entry->left = gpu_host_op_map_root;
            entry->right = gpu_host_op_map_root->right;
            gpu_host_op_map_root->right = NULL;
        } else {
            // host_op_id already present: fatal error since a host_op_id 
            //   should only be inserted once 
            assert(0);
        }
    }

    gpu_host_op_map_root = entry;
}


bool
gpu_host_op_map_samples_increase(uint64_t host_op_id, int val)
{
    bool result = true;

    TMSG(DEFER_CTXT, "host op map samples update: id=0x%lx (update %d)",
         host_op_id, val);

    gpu_host_op_map_root = gpu_host_op_map_splay(gpu_host_op_map_root, host_op_id);

    if (gpu_host_op_map_root && gpu_host_op_map_root->host_op_id == host_op_id) {
        gpu_host_op_map_root->samples += val;
        PRINT("total %d curr %d\n", gpu_host_op_map_root->total_samples, gpu_host_op_map_root->samples);
        if (gpu_host_op_map_root->samples == gpu_host_op_map_root->total_samples) {
            PRINT("sample root deleted\n");
            gpu_host_op_map_delete_root();
            result = false;
        }
    }

    return result;
}


bool
gpu_host_op_map_total_samples_update(uint64_t host_op_id, int val)
{
    bool result = true;

    TMSG(DEFER_CTXT, "host op map total samples update: id=0x%lx (update %d)",
         host_op_id, val);

    gpu_host_op_map_root = gpu_host_op_map_splay(gpu_host_op_map_root, host_op_id);

    PRINT("gpu_host_op_map_total_samples_update\n");
    if (gpu_host_op_map_root && gpu_host_op_map_root->host_op_id == host_op_id) {
        gpu_host_op_map_root->total_samples = val;
        PRINT("total %d curr %d\n", gpu_host_op_map_root->total_samples, gpu_host_op_map_root->samples);
        if (gpu_host_op_map_root->samples == gpu_host_op_map_root->total_samples) {
            PRINT("sample root deleted\n");
            gpu_host_op_map_delete_root();
            result = false;
        }
    }

    return result;
}


void
gpu_host_op_map_delete(uint64_t host_op_id)
{
    gpu_host_op_map_root = gpu_host_op_map_splay(gpu_host_op_map_root, host_op_id);

    if (gpu_host_op_map_root && gpu_host_op_map_root->host_op_id == host_op_id) {
        gpu_host_op_map_delete_root();
    }
}


cct_node_t *
gpu_host_op_map_entry_host_op_node_get(gpu_host_op_map_entry_t *entry)
{
    return entry->host_op_node;
}


gpu_record_t *
gpu_host_op_map_entry_record_get(gpu_host_op_map_entry_t *entry)
{
    return entry->record;
}

entry_data_t *
gpu_host_op_map_entry_data_get
        (
                gpu_host_op_map_entry_t *entry
        )
{
    return entry->data;
}


/******************************************************************************
 * debugging code
 *****************************************************************************/

static int
gpu_host_op_map_count_helper(gpu_host_op_map_entry_t *entry)
{
    if (entry) {
        int left = gpu_host_op_map_count_helper(entry->left);
        int right = gpu_host_op_map_count_helper(entry->right);
        return 1 + right + left;
    }
    return 0;
}


int
gpu_host_op_map_count()
{
    return gpu_host_op_map_count_helper(gpu_host_op_map_root);
}
