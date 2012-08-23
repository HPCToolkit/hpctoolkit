// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/branches/hpctoolkit-gpu-blame-shift-proto/src/tool/hpcrun/write_stream_data.c $
// $Id: write_data.c 3650 2012-01-18 21:02:40Z krentel $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2011, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//*****************************************************************************
// system includes
//*****************************************************************************

//*****************************************************************************
// local includes
//*****************************************************************************

#include "stream.h"

static epoch_flags_t stream_epoch_flags = {
  .bits = 0
};

static const uint64_t stream_default_measurement_granularity = 1;

static const uint32_t stream_default_ra_to_callsite_distance =
#if defined(HOST_PLATFORM_MIPS64LE_LINUX)
  8 // move past branch delay slot
#else
  1 // probably sufficient all architectures without a branch-delay slot
#endif
  ;


//*****************************************************************************
// structs and types
//*****************************************************************************
static FILE * open_stream_data_file(stream_data_t *st)
{
  thread_data_t* td = hpcrun_get_thread_data();

  FILE* f = st->hpcrun_file;
  if (f) {
    return f;
  }

  /*
 * FIXME: open_profile_file has to be changed but for now lets assume
 * that this is bigger than the number of processes
 */
	int rank = hpcrun_get_rank();//td->rank;//st->device_id; 
	if (rank < 0) {
		rank = 0;
	}
  int fd = hpcrun_open_profile_file(rank, st->id);

  f = fdopen(fd, "w");
  if (f == NULL) {
    EEMSG("HPCToolkit: %s: unable to open profile file", __func__);
    return NULL;
  }
  st->hpcrun_file = f;

  if (! hpcrun_sample_prob_active())
    return f;

  const uint bufSZ = 32; // sufficient to hold a 64-bit integer in base 10

  const char* jobIdStr = OSUtil_jobid();
  if (!jobIdStr) {
    jobIdStr = "";
  }

  char mpiRankStr[bufSZ];
  mpiRankStr[0] = '\0';
  snprintf(mpiRankStr, bufSZ, "%d", rank);

  char tidStr[bufSZ];
  snprintf(tidStr, bufSZ, "%d", st->id);

  char hostidStr[bufSZ];
  snprintf(hostidStr, bufSZ, "%lx", OSUtil_hostid());

  char pidStr[bufSZ];
  snprintf(pidStr, bufSZ, "%u", OSUtil_pid());

  char traceMinTimeStr[bufSZ];
  snprintf(traceMinTimeStr, bufSZ, "%"PRIu64, st->trace_min_time_us);
	//printf("\nThe start time  is %x for stream %d",st->trace_min_time_us, st->id);

  char traceMaxTimeStr[bufSZ];
  snprintf(traceMaxTimeStr, bufSZ, "%"PRIu64, st->trace_max_time_us);
	//printf("\nThe end time  is %x for stream %d",st->trace_max_time_us, st->id);

  //
  // ==== file hdr =====
  //

  TMSG(DATA_WRITE,"writing file header");
  hpcrun_fmt_hdr_fwrite(f,
                        HPCRUN_FMT_NV_prog, hpcrun_files_executable_name(),
                        HPCRUN_FMT_NV_progPath, hpcrun_files_executable_pathname(),
			HPCRUN_FMT_NV_envPath, getenv("PATH"),
                        HPCRUN_FMT_NV_jobId, jobIdStr,
                        HPCRUN_FMT_NV_mpiRank, mpiRankStr,
                        HPCRUN_FMT_NV_tid, tidStr,
                        HPCRUN_FMT_NV_hostid, hostidStr,
                        HPCRUN_FMT_NV_pid, pidStr,
			HPCRUN_FMT_NV_traceMinTime, traceMinTimeStr,
			HPCRUN_FMT_NV_traceMaxTime, traceMaxTimeStr,
                        NULL);
  return f;
}

typedef void (*stream_cct_op_t)(stream_data_t *st, cct_node_t* cct, cct_op_arg_t arg, size_t level);

static void
stream_lwrite(stream_data_t *st, cct_node_t* node, cct_op_arg_t arg, size_t level)
{
    write_arg_t* my_arg = (write_arg_t*) arg;
    hpcrun_fmt_cct_node_t* tmp = my_arg->tmp_node;
    cct_node_t* parent = hpcrun_cct_parent(node);
    epoch_flags_t flags = my_arg->flags;
    cct_addr_t* addr    = hpcrun_cct_addr(node);
    
    tmp->id = hpcrun_cct_persistent_id(node);
    tmp->id_parent = parent ? hpcrun_cct_persistent_id(parent) : 0;
    
    // if leaf, chg sign of id when written out
    if (hpcrun_cct_is_leaf(node)) {
        tmp->id = - tmp->id;
    }
    if (flags.fields.isLogicalUnwind){
        tmp->as_info = addr->as_info;
        lush_lip_init(&tmp->lip);
        if (addr->lip) {
            memcpy(&(tmp->lip), &(addr->lip), sizeof(lush_lip_t));
        }
    }
    tmp->lm_id = (addr->ip_norm).lm_id;
    
    // double casts to avoid warnings when pointer is < 64 bits 
    tmp->lm_ip = (hpcfmt_vma_t) (uintptr_t) (addr->ip_norm).lm_ip;
    
    tmp->num_metrics = my_arg->num_metrics;
    hpcrun_metric_set_dense_copy(tmp->metrics, hpcrun_get_metric_set(node),
                                 my_arg->num_metrics);
    hpcrun_fmt_cct_node_fwrite(tmp, flags, my_arg->fs);
}


static void
stream_walk_child_lrs(stream_data_t *st, cct_node_t* cct, 
               stream_cct_op_t op, cct_op_arg_t arg, size_t level,
               void (*wf)(stream_data_t *st, cct_node_t* n, stream_cct_op_t o, cct_op_arg_t a, size_t l))
{
    if (!cct) return;
    
    stream_walk_child_lrs(st, cct->left, op, arg, level, wf);
    stream_walk_child_lrs(st, cct->right, op, arg, level, wf);
    wf(st, cct, op, arg, level);
}


void
hpcrun_stream_cct_walk_node_1st_w_level(stream_data_t *st, cct_node_t* cct, stream_cct_op_t op, cct_op_arg_t arg, size_t level)
{
    if (!cct) return;
    op(st, cct, arg, level);
    stream_walk_child_lrs(st, cct->children, op, arg, level+1,
                   hpcrun_stream_cct_walk_node_1st_w_level);
}


int
hpcrun_stream_cct_fwrite(stream_data_t *st, cct_node_t* cct, FILE* fs, epoch_flags_t flags)
{
    if (!fs) return HPCRUN_ERR;
    
    hpcfmt_int8_fwrite((uint64_t) hpcrun_cct_num_nodes(cct), fs);
    TMSG(DATA_WRITE, "num cct nodes = %d", hpcrun_cct_num_nodes(cct));
    
    hpcfmt_uint_t num_metrics = hpcrun_get_num_metrics();
    TMSG(DATA_WRITE, "num metrics in a cct node = %d", num_metrics);
    
    hpcrun_fmt_cct_node_t tmp_node;
    
    write_arg_t write_arg = {
        .num_metrics = num_metrics,
        .fs          = fs,
        .flags       = flags,
        .tmp_node    = &tmp_node,
    };
    
    hpcrun_metricVal_t metrics[num_metrics];
    tmp_node.metrics = &(metrics[0]);
    
    hpcrun_stream_cct_walk_node_1st_w_level(st, cct, stream_lwrite, &write_arg, 0);
    
    return HPCRUN_OK;
}


int 
hpcrun_stream_cct_bundle_fwrite(stream_data_t *st, FILE* fs, epoch_flags_t flags, cct_bundle_t* bndl)
{
    if (!fs) { return HPCRUN_ERR; }
    
    cct_node_t* final = bndl->tree_root;
    cct_node_t* partial_insert = final;
    hpcrun_cct_insert_node(bndl->partial_unw_root, bndl->special_idle_node);
    hpcrun_cct_insert_node(partial_insert, bndl->partial_unw_root);
 
    return hpcrun_stream_cct_fwrite(st, bndl->top, fs, flags);
}


static int
write_stream_epoch(FILE* f, stream_data_t *st)
{
  //uint32_t num_epochs = 0;
  if (! hpcrun_sample_prob_active())
    return HPCRUN_OK;
	epoch_t* current_epoch = st->epoch;

	stream_epoch_flags.fields.isLogicalUnwind = hpcrun_isLogicalUnwind();
	TMSG(LUSH,"epoch lush flag set to %s", stream_epoch_flags.fields.isLogicalUnwind ? "true" : "false");

	TMSG(DATA_WRITE,"epoch flags = %"PRIx64"", stream_epoch_flags.bits);
	hpcrun_fmt_epochHdr_fwrite(f, stream_epoch_flags,
									stream_default_measurement_granularity,
									stream_default_ra_to_callsite_distance,
									"TODO:epoch-name","TODO:epoch-value",
									NULL);

	metric_desc_p_tbl_t *metric_tbl = hpcrun_get_metric_tbl();

	TMSG(DATA_WRITE, "metric tbl len = %d", metric_tbl->len);
	hpcrun_fmt_metricTbl_fwrite(metric_tbl, f);

	TMSG(DATA_WRITE, "Done writing metric data");
	TMSG(DATA_WRITE, "Preparing to write loadmap");

	hpcrun_loadmap_t* current_loadmap = current_epoch->loadmap;

	hpcfmt_int4_fwrite(current_loadmap->size, f);

	// N.B.: Write in reverse order to obtain nicely ascending LM ids.
	for (load_module_t* lm_src = current_loadmap->lm_end;
									(lm_src); lm_src = lm_src->prev) {
					loadmap_entry_t lm_entry;
					lm_entry.id = lm_src->id;
					lm_entry.name = lm_src->name;
					lm_entry.flags = 0;

					hpcrun_fmt_loadmapEntry_fwrite(&lm_entry, f);
	}

	TMSG(DATA_WRITE, "Done writing loadmap");

	cct_bundle_t* cct      = &(current_epoch->csdata);
	//int i= (rand()) % 100;
	//hpcrun_get_metric_proc(0)(0, hpcrun_reify_metric_set(cct), (cct_metric_data_t){.i = i});

	/*FIXME: hpcrun_cct_bundle_fwrite does a lot more than just writing out to file
 */
	int ret = hpcrun_stream_cct_bundle_fwrite(st, f, stream_epoch_flags, cct);
	if(ret != HPCRUN_OK) {
					TMSG(DATA_WRITE, "Error writing tree %#lx", cct);
					TMSG(DATA_WRITE, "Number of tree nodes lost: %ld", cct->num_nodes);
					EMSG("could not save profile data to hpcrun file");
					perror("write_profile_data");
					ret = HPCRUN_ERR; // FIXME: return this value now
	}
	else {
					TMSG(DATA_WRITE, "saved profile data to hpcrun file ");
	}
	return HPCRUN_OK;
}


int
hpcrun_write_stream_profile_data(stream_data_t *st)
{
	//printf("\nABOUT TO WRITE THE STREAM PROFILE DATA");
  TMSG(DATA_WRITE,"Writing stream hpcrun profile data");
  FILE* f = open_stream_data_file(st);
  if (f == NULL)
    return HPCRUN_ERR;

  write_stream_epoch(f, st);

  TMSG(DATA_WRITE,"closing file");
  hpcio_fclose(f);
  TMSG(DATA_WRITE,"Done!");

  return HPCRUN_OK;
}
