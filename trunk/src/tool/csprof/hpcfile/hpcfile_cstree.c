// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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

//***************************************************************************
//
// File:
//    hpcfile_cstree.c
//
// Purpose:
//    Low-level types and functions for reading/writing a call stack
//    tree from/to a binary file.
//
//    These routines *must not* allocate dynamic memory; if such memory
//    is needed, callbacks to the user's allocator should be used.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>
#include <assert.h>

//*************************** User Include Files ****************************

#include "hpcfile_general.h"
#include "hpcfile_cstreelib.h"
#include "hpcfile_cstree.h"

//#include <lib/support/diagnostics.h>

//*************************** Forward Declarations **************************

#define HPCFILE_TAG__CSTREE_NODE 13 /* just because */
#define HPCFILE_TAG__CSTREE_LIP  77 /* feel free to change */

hpcfile_metric_data_t hpcfile_metric_data_ZERO = { .bits = 0 };

//*************************** Forward Declarations **************************

#define DBG_READ_METRICS 0

static int 
hpcfile_cstree_read_hdr(FILE* fs, hpcfile_cstree_hdr_t* hdr);

// HPC_CSTREE format details: 
//
// As the nodes are written, each will be given a persistent id and
// each node will know its parent's persistent id.  The id numbers are
// between CSTREE_ID_ROOT and (CSTREE_ID_ROOT + n - 1) where n is the
// number of nodes in the tree.  Furthermore, parents are always
// written before any of their children.  This scheme allows
// hpcfile_cstree_read to easily and efficiently construct the tree
// since the persistent ids correspond to simple array indices and
// parent nodes will be read before children.
//
// The id's will be will be assigned so that given a node x and its
// children, the id of x is less than the id of any child and less
// than the id of any child's descendent.

// ---------------------------------------------------------
// Three cases for lips:
//   1) a-to-0:         trivial
//   2) M-to-1, 1-to-1: associate lip with root note
//   3) 1-to-M:         associate lip with each note
//
// 1. If a lip is necessary, it comes *after* the note (since note
//    association indicates whether a lip is needed) and receives the
//    id of that node.
//
// 2. We can pass chord root ids as an extra parameter in PREORDER walk.



//***************************************************************************
// hpcfile_cstree_read()
//***************************************************************************

// See header file for documentation of public interface.
// Cf. 'HPC_CSTREE format details' above.
int
hpcfile_cstree_read(FILE* fs, void* tree, 
		    int num_metrics,
		    hpcfile_cstree_cb__create_node_fn_t create_node_fn,
		    hpcfile_cstree_cb__link_parent_fn_t link_parent_fn,
		    hpcfile_cb__alloc_fn_t alloc_fn,
		    hpcfile_cb__free_fn_t free_fn)
{
  hpcfile_cstree_hdr_t fhdr;
  hpcfile_cstree_node_t tmp_node;
  int ret = HPCFILE_ERR;
  uint32_t tag;
  
  // A vector storing created tree nodes.  The vector indices correspond to
  // the nodes' persistent ids.
  void**       node_vec = NULL;
  lush_lip_t** lip_vec  = NULL;

  if (!fs) { return HPCFILE_ERR; }
  
  // Open file for reading; read and sanity check header
  if (hpcfile_cstree_read_hdr(fs, &fhdr) != HPCFILE_OK) { 
    return HPCFILE_ERR; 
  }

  // node id upper bound (exclusive)
  unsigned int id_ub = fhdr.num_nodes + HPCFILE_CSTREE_ID_ROOT;

  // Allocate space for 'node_vec'
  if (fhdr.num_nodes != 0) {
    node_vec = alloc_fn(sizeof(void*) * id_ub);
    lip_vec  = alloc_fn(sizeof(void*) * id_ub);
    for (int i = 0; i < HPCFILE_CSTREE_ID_ROOT; ++i) {
      node_vec[i] = NULL;
      lip_vec[i]  = NULL;
    }
  }
  
  // Read each node, creating it and linking it to its parent 
  tmp_node.data.num_metrics = num_metrics;
  tmp_node.data.metrics = alloc_fn(num_metrics * sizeof(hpcfile_uint_t));
  
  for (int i = HPCFILE_CSTREE_ID_ROOT; i < id_ub; ++i) {

    ret = hpcfile_tag__fread(&tag, fs);
    if (ret != HPCFILE_OK) { return HPCFILE_ERR; }

    // ----------------------------------------------------------
    // Read the LIP
    // ----------------------------------------------------------
    lush_lip_t* lip = NULL;
    hpcfile_uint_t lip_idx = i;
    
    if (tag == HPCFILE_TAG__CSTREE_LIP) {
      lip = alloc_fn(sizeof(lush_lip_t));
      ret = hpcfile_cstree_lip__fread(lip, fs);
      if (ret != HPCFILE_OK) { return HPCFILE_ERR; }

      ret = hpcfile_tag__fread(&tag, fs);
      if (ret != HPCFILE_OK) { return HPCFILE_ERR; }
    }

    lip_vec[lip_idx] = lip;

    // ----------------------------------------------------------
    // Read the node
    // ----------------------------------------------------------
    
    if ( !(tag == HPCFILE_TAG__CSTREE_NODE) ) {
      goto cstree_read_cleanup; // HPCFILE_ERR
    }

    ret = hpcfile_cstree_node__fread(&tmp_node, fs);
    if (ret != HPCFILE_OK) {
      goto cstree_read_cleanup; // HPCFILE_ERR
    }

    lip_idx = tmp_node.data.lip.id;
    tmp_node.data.lip.ptr = lip_vec[lip_idx];

    if (tmp_node.id_parent >= id_ub) { 
      goto cstree_read_cleanup; // HPCFILE_ERR
    } 
    void* parent = node_vec[tmp_node.id_parent];

    // parent should already exist unless id == HPCFILE_CSTREE_ID_ROOT
    if (!parent && tmp_node.id != HPCFILE_CSTREE_ID_ROOT) {
      goto cstree_read_cleanup; // HPCFILE_ERR
    }

    // Create node and link to parent
    void* node = create_node_fn(tree, &tmp_node.data);
    node_vec[tmp_node.id] = node;

    if (parent) {
      link_parent_fn(tree, node, parent);
    }
  }

  free_fn(tmp_node.data.metrics);


  // Success! Note: We assume that it is possible for other data to
  // exist beyond this point in the stream; don't check for EOF.
  ret = HPCFILE_OK;
  
  // Close file and cleanup
 cstree_read_cleanup:
  if (node_vec) { free_fn(node_vec); }
  if (lip_vec)  { free_fn(lip_vec); }
  
  return ret;
}

//***************************************************************************
// hpcfile_cstree_fprint()
//***************************************************************************

// See header file for documentation of public interface.
int
hpcfile_cstree_fprint(FILE* infs, int num_metrics, FILE* outfs)
{
  // FIXME: could share some code with the reader
  
  hpcfile_cstree_hdr_t fhdr;
  hpcfile_cstree_node_t tmp_node;
  int i, ret;
  uint32_t tag;
  
  // Open file for reading; read and sanity check header
  if (hpcfile_cstree_read_hdr(infs, &fhdr) != HPCFILE_OK) {
    fprintf(outfs, "** Error reading header **\n");
    return HPCFILE_ERR;
  }
  
  // Print header
  hpcfile_cstree_hdr__fprint(&fhdr, outfs);
  fputs("\n", outfs);

  // Read and print each node
  unsigned int id_ub = fhdr.num_nodes + HPCFILE_CSTREE_ID_ROOT;

  tmp_node.data.num_metrics = num_metrics;
  tmp_node.data.metrics = malloc(num_metrics * sizeof(hpcfile_uint_t));

  for (i = HPCFILE_CSTREE_ID_ROOT; i < id_ub; ++i) {

    ret = hpcfile_tag__fread(&tag, infs);
    if (ret != HPCFILE_OK) { return HPCFILE_ERR; }

    // ----------------------------------------------------------
    // Read the LIP
    // ----------------------------------------------------------
    if (tag == HPCFILE_TAG__CSTREE_LIP) {
      lush_lip_t lip;
      ret = hpcfile_cstree_lip__fread(&lip, infs);
      if (ret != HPCFILE_OK) { 	
	fprintf(outfs, "** Error reading LIP %d **\n", i);
	goto cstree_read_cleanup; // HPCFILE_ERR
      }

      hpcfile_cstree_lip__fprint(&lip, i, outfs, "");

      ret = hpcfile_tag__fread(&tag, infs);
      if (ret != HPCFILE_OK) { 
	goto cstree_read_cleanup; // HPCFILE_ERR
      }
    }

    // ----------------------------------------------------------
    // Read the node
    // ----------------------------------------------------------
    ret = hpcfile_cstree_node__fread(&tmp_node, infs);
    if (ret != HPCFILE_OK) { 
      fprintf(outfs, "** Error reading node %d **\n", i);
      goto cstree_read_cleanup; // HPCFILE_ERR
    }

    hpcfile_cstree_node__fprint(&tmp_node, outfs, "  ");
  }

  ret = HPCFILE_OK; // Success!

 cstree_read_cleanup:
  free(tmp_node.data.metrics);

  // Success! Note: We assume that it is possible for other data to
  // exist beyond this point in the stream; don't check for EOF.
  return ret;
}

//***************************************************************************

// hpcfile_cstree_read_hdr: Read the cstree header from the file
// stream 'fs' into 'hdr' and sanity check header info.  Returns
// HPCFILE_OK upon success; HPCFILE_ERR on error.
int
hpcfile_cstree_read_hdr(FILE* fs, hpcfile_cstree_hdr_t* hdr)
{
  // Read header
  if (hpcfile_cstree_hdr__fread(hdr, fs) != HPCFILE_OK) { 
    return HPCFILE_ERR; 
  }
  
  // Sanity check file id
  if (strncmp(hdr->fid.magic_str, HPCFILE_CSTREE_MAGIC_STR, 
	      HPCFILE_CSTREE_MAGIC_STR_LEN) != 0) { 
    return HPCFILE_ERR; 
  }
  if (strncmp(hdr->fid.version, HPCFILE_CSTREE_VERSION, 
	      HPCFILE_CSTREE_VERSION_LEN) != 0) { 
    return HPCFILE_ERR; 
  }
  if (hdr->fid.endian != HPCFILE_CSTREE_ENDIAN) { return HPCFILE_ERR; }
  
  // Sanity check header
  if (hdr->vma_sz != 8) { return HPCFILE_ERR; }
  if (hdr->uint_sz != 8) { return HPCFILE_ERR; }
  
  return HPCFILE_OK;
}

//***************************************************************************

int 
hpcfile_cstree_id__init(hpcfile_cstree_id_t* x)
{
  memset(x, 0, sizeof(*x));

  strncpy(x->magic_str, HPCFILE_CSTREE_MAGIC_STR, HPCFILE_CSTREE_MAGIC_STR_LEN);
  strncpy(x->version, HPCFILE_CSTREE_VERSION, HPCFILE_CSTREE_VERSION_LEN);
  x->endian = HPCFILE_CSTREE_ENDIAN;

  return HPCFILE_OK;
}

int 
hpcfile_cstree_id__fini(hpcfile_cstree_id_t* x)
{
  return HPCFILE_OK;
}

int 
hpcfile_cstree_id__fread(hpcfile_cstree_id_t* x, FILE* fs)
{
  size_t sz;
  int c;

  sz = fread((char*)x->magic_str, 1, HPCFILE_CSTREE_MAGIC_STR_LEN, fs);
  if (sz != HPCFILE_CSTREE_MAGIC_STR_LEN) { return HPCFILE_ERR; }
  
  sz = fread((char*)x->version, 1, HPCFILE_CSTREE_VERSION_LEN, fs);
  if (sz != HPCFILE_CSTREE_VERSION_LEN) { return HPCFILE_ERR; }

  c = fgetc(fs);
  if (c == EOF) { return HPCFILE_ERR; }
  x->endian = (char)c;

  return HPCFILE_OK;
}

int 
hpcfile_cstree_id__fwrite(hpcfile_cstree_id_t* x, FILE* fs)
{
  size_t sz;

  sz = fwrite((char*)x->magic_str, 1, HPCFILE_CSTREE_MAGIC_STR_LEN, fs);
  if (sz != HPCFILE_CSTREE_MAGIC_STR_LEN) { return HPCFILE_ERR; }

  sz = fwrite((char*)x->version, 1, HPCFILE_CSTREE_VERSION_LEN, fs);
  if (sz != HPCFILE_CSTREE_VERSION_LEN) { return HPCFILE_ERR; }
  
  if (fputc(x->endian, fs) == EOF) { return HPCFILE_ERR; }

  return HPCFILE_OK;
}

int 
hpcfile_cstree_id__fprint(hpcfile_cstree_id_t* x, FILE* fs, const char* pre)
{
  fprintf(fs, "%s{fileid: (magic: %s) (ver: %s) (end: %c)}\n", 
	  pre, x->magic_str, x->version, x->endian);

  return HPCFILE_OK;
}

//***************************************************************************

int 
hpcfile_cstree_hdr__init(hpcfile_cstree_hdr_t* x)
{
  memset(x, 0, sizeof(*x));
  hpcfile_cstree_id__init(&x->fid);
  
  x->vma_sz  = sizeof(hpcfile_vma_t);
  x->uint_sz = sizeof(hpcfile_uint_t);

  return HPCFILE_OK;
}

int 
hpcfile_cstree_hdr__fini(hpcfile_cstree_hdr_t* x)
{
  hpcfile_cstree_id__fini(&x->fid);
  return HPCFILE_OK;
}

int 
hpcfile_cstree_hdr__fread(hpcfile_cstree_hdr_t* x, FILE* fs)
{
  size_t sz;
    
  if (hpcfile_cstree_id__fread(&x->fid, fs) != HPCFILE_OK) { return HPCFILE_ERR; }
  
  sz = hpc_fread_le4(&x->vma_sz, fs);
  if (sz != sizeof(x->vma_sz)) { return HPCFILE_ERR; }
  
  sz = hpc_fread_le4(&x->uint_sz, fs);
  if (sz != sizeof(x->uint_sz)) { return HPCFILE_ERR; }
  
  sz = hpc_fread_le8(&x->num_nodes, fs);
  if (sz != sizeof(x->num_nodes)) { return HPCFILE_ERR; }

  sz = hpc_fread_le4(&x->epoch, fs);
  if (sz != sizeof(x->epoch)) { return HPCFILE_ERR; }

  return HPCFILE_OK;
}

int 
hpcfile_cstree_hdr__fwrite(hpcfile_cstree_hdr_t* x, FILE* fs)
{
  size_t sz;
  
  if (hpcfile_cstree_id__fwrite(&x->fid, fs) != HPCFILE_OK) { return HPCFILE_ERR; }

  sz = hpc_fwrite_le4(&x->vma_sz, fs);
  if (sz != sizeof(x->vma_sz)) { return HPCFILE_ERR; }
 
  sz = hpc_fwrite_le4(&x->uint_sz, fs);
  if (sz != sizeof(x->uint_sz)) { return HPCFILE_ERR; }

  sz = hpc_fwrite_le8(&x->num_nodes, fs);
  if (sz != sizeof(x->num_nodes)) { return HPCFILE_ERR; }

  sz = hpc_fwrite_le4(&x->epoch, fs);
  if (sz != sizeof(x->epoch)) { return HPCFILE_ERR; }

  return HPCFILE_OK;
}

int 
hpcfile_cstree_hdr__fprint(hpcfile_cstree_hdr_t* x, FILE* fs)
{
  const char* pre = "  ";
  fputs("{cstree_hdr:\n", fs);

  hpcfile_cstree_id__fprint(&x->fid, fs, pre);

  fprintf(fs, "%s(vma_sz: %u) (uint_sz: %u) (num_nodes: %"PRIu64")}\n", 
	  pre, x->vma_sz, x->uint_sz, x->num_nodes);
  
  return HPCFILE_OK;
}

//***************************************************************************

int 
hpcfile_cstree_nodedata__init(hpcfile_cstree_nodedata_t* x)
{
  memset(x, 0, sizeof(*x));
  return HPCFILE_OK;
}

int 
hpcfile_cstree_nodedata__fini(hpcfile_cstree_nodedata_t* x)
{
  return HPCFILE_OK;
}

int 
hpcfile_cstree_nodedata__fread(hpcfile_cstree_nodedata_t* x, FILE* fs)
{
  // ASSUMES: space for metrics has been allocated
  
  size_t sz;
  int i, ret;

  ret = hpcfile_cstree_as_info__fread(&x->as_info, fs);
  if (ret != HPCFILE_OK) { return HPCFILE_ERR; }

  sz = hpc_fread_le8(&x->ip, fs);
  if (sz != sizeof(x->ip)) { return HPCFILE_ERR; }

  sz = hpc_fread_le8(&x->lip.id, fs);
  if (sz != sizeof(x->lip.id)) { return HPCFILE_ERR; }

  sz = hpc_fread_le8(&x->sp, fs);
  if (sz != sizeof(x->sp)) { return HPCFILE_ERR; }

  //DIAG_MsgIf(DBG_READ_METRICS, "reading node ip=%"PRIx64, x->ip);
  for (i = 0; i < x->num_metrics; ++i) {
    sz = hpc_fread_le8(&x->metrics[i].bits, fs);
    //DIAG_MsgIf(DBG_READ_METRICS, "metrics[%d]=%"PRIu64, i, x->metrics[i]);
    if (sz != sizeof(x->metrics[i])) {
      return HPCFILE_ERR;
    }
  }

  return HPCFILE_OK;
}


int 
hpcfile_cstree_nodedata__fwrite(hpcfile_cstree_nodedata_t* x, FILE* fs) 
{
  size_t sz;
  int i, ret;

  ret = hpcfile_cstree_as_info__fwrite(&x->as_info, fs);
  if (ret != HPCFILE_OK) { return HPCFILE_ERR; }

  sz = hpc_fwrite_le8(&x->ip, fs);
  if (sz != sizeof(x->ip)) { return HPCFILE_ERR; }

  sz = hpc_fwrite_le8(&x->lip.id, fs);
  if (sz != sizeof(x->lip.id)) { return HPCFILE_ERR; }

  sz = hpc_fwrite_le8(&x->sp, fs);
  if (sz != sizeof(x->sp)) { return HPCFILE_ERR; }

  for (i = 0; i < x->num_metrics; ++i) {
    sz = hpc_fwrite_le8(&x->metrics[i].bits, fs);
    if (sz != sizeof(x->metrics[i])) {
      return HPCFILE_ERR;
    }
  }

  return HPCFILE_OK;
}

int 
hpcfile_cstree_nodedata__fprint(hpcfile_cstree_nodedata_t* x, FILE* fs, 
				const char* pre)
{
  char as_str[LUSH_ASSOC_INFO_STR_MIN_LEN];
  lush_assoc_info_sprintf(as_str, x->as_info);

  fprintf(fs, "%s{nodedata: (as: %s) (ip: 0x%"PRIx64") (lip: [%"PRIu64"][%p]) (sp: %"PRIx64")\n", pre, as_str, x->ip, x->lip.id, x->lip.ptr, x->sp);

  fprintf(fs, "%s  (metrics:", pre);
  for (int i = 0; i < x->num_metrics; ++i) {
    fprintf(fs, " %"PRIu64" ", x->metrics[i].bits);
  }
  fprintf(fs, ") }\n");
  
  return HPCFILE_OK;
}

//***************************************************************************

int 
hpcfile_cstree_as_info__fread(lush_assoc_info_t* x, FILE* fs)
{
  size_t sz;

  sz = hpc_fread_le4(&x->bits, fs);
  if (sz != sizeof(x->bits)) { return HPCFILE_ERR; }
  
  return HPCFILE_OK;
}

int 
hpcfile_cstree_as_info__fwrite(lush_assoc_info_t* x, FILE* fs)
{
  size_t sz;

  sz = hpc_fwrite_le4(&x->bits, fs);
  if (sz != sizeof(x->bits)) { return HPCFILE_ERR; }

  return HPCFILE_OK;
}

int 
hpcfile_cstree_as_info__fprint(lush_lip_t* x, FILE* fs, const char* pre)
{
  // not done
  return HPCFILE_OK;
}

//***************************************************************************

int 
hpcfile_cstree_lip__fread(lush_lip_t* x, FILE* fs)
{
  size_t sz;

  //  HPCFILE_TAG__CSTREE_LIP has already been read

  for (int i = 0; i < LUSH_LIP_DATA8_SZ; ++i) {
    sz = hpc_fread_le8(&x->data8[i], fs);
    if (sz != sizeof(x->data8[i])) { return HPCFILE_ERR; }
  }
  
  return HPCFILE_OK;
}

int 
hpcfile_cstree_lip__fwrite(lush_lip_t* x, FILE* fs)
{
  size_t sz;
  int ret;

  ret = hpcfile_tag__fwrite(HPCFILE_TAG__CSTREE_LIP, fs);
  if (ret != HPCFILE_OK) {
    return HPCFILE_ERR; 
  }

  for (int i = 0; i < LUSH_LIP_DATA8_SZ; ++i) {
    sz = hpc_fwrite_le8(&x->data8[i], fs);
    if (sz != sizeof(x->data8[i])) { return HPCFILE_ERR; }
  }
  
  return HPCFILE_OK;
}

int 
hpcfile_cstree_lip__fprint(lush_lip_t* x, hpcfile_uint_t id, 
			   FILE* fs, const char* pre)
{
  char lip_str[LUSH_LIP_STR_MIN_LEN];
  lush_lip_sprintf(lip_str, x);

  fprintf(fs, "%s{lip:  (id: %"PRIu64") %s}\n", pre, id, lip_str);

  return HPCFILE_OK;
}


//***************************************************************************

int 
hpcfile_cstree_node__init(hpcfile_cstree_node_t* x)
{
  memset(x, 0, sizeof(*x));
  hpcfile_cstree_nodedata__init(&x->data);
  return HPCFILE_OK;
}

int 
hpcfile_cstree_node__fini(hpcfile_cstree_node_t* x)
{
  hpcfile_cstree_nodedata__fini(&x->data);  
  return HPCFILE_OK;
}

int 
hpcfile_cstree_node__fread(hpcfile_cstree_node_t* x, FILE* fs)
{
  size_t sz;
  int ret;

  // HPCFILE_TAG__CSTREE_NODE has already been read

  sz = hpc_fread_le8(&x->id, fs);
  if (sz != sizeof(x->id)) { return HPCFILE_ERR; }
  
  sz = hpc_fread_le8(&x->id_parent, fs);
  if (sz != sizeof(x->id_parent)) { return HPCFILE_ERR; }
  
  ret = hpcfile_cstree_nodedata__fread(&x->data, fs);
  if (ret != HPCFILE_OK) {
    return HPCFILE_ERR; 
  }
  
  return HPCFILE_OK;
}

int 
hpcfile_cstree_node__fwrite(hpcfile_cstree_node_t* x, FILE* fs)
{
  size_t sz;
  int ret;

  ret = hpcfile_tag__fwrite(HPCFILE_TAG__CSTREE_NODE, fs);
  if (ret != HPCFILE_OK) {
    return HPCFILE_ERR; 
  }

  sz = hpc_fwrite_le8(&x->id, fs);
  if (sz != sizeof(x->id)) { return HPCFILE_ERR; }
  
  sz = hpc_fwrite_le8(&x->id_parent, fs);
  if (sz != sizeof(x->id_parent)) { return HPCFILE_ERR; }
  
  if (hpcfile_cstree_nodedata__fwrite(&x->data, fs) != HPCFILE_OK) {
    return HPCFILE_ERR; 
  }
  
  return HPCFILE_OK;
}

int 
hpcfile_cstree_node__fprint(hpcfile_cstree_node_t* x, FILE* fs, const char* pre)
{
  fprintf(fs, "{node: (id: %"PRIu64") (id_parent: %"PRIu64")}\n", 
	  x->id, x->id_parent);

  hpcfile_cstree_nodedata__fprint(&x->data, fs, pre);
  
  return HPCFILE_OK;
}

//***************************************************************************

