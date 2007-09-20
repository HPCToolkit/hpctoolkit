// $Id$
// -*-C-*-
// * BeginRiceCopyright *****************************************************
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

//*************************** User Include Files ****************************

#include "hpcfile_general.h"
#include "hpcfile_cstreelib.h"
#include "hpcfile_cstree.h"

//*************************** Forward Declarations **************************

static int
hpcfile_cstree_write_node(FILE* fs, void* tree, void* node, 
			  hpcfile_cstree_node_t *scratch,
			  hpcfile_uint_t id_parent,
			  hpcfile_cstree_cb__get_data_fn_t get_node_data_fn,
			  hpcfile_cstree_cb__get_first_child_fn_t get_first_child_fn,
			  hpcfile_cstree_cb__get_sibling_fn_t get_sibling_fn);

static int hpcfile_cstree_read_hdr(FILE* fs, hpcfile_cstree_hdr_t* hdr);

// HPC_CSTREE format details: 
//
// As the nodes are written, each will be given a persistent id and
// each node will know its parent's persistent id.  The id numbers are
// between 0 and n-1 where n is the number of nodes in the tree.
// Furthermore, parents are always written before any of their
// children.  This scheme allows hpcfile_cstree_read to easily and efficiently
// construct the tree since the persistent ids correspond to simple
// array indices and parent nodes will be read before children.
//
// The id's will be will be assigned so that given a node x and its
// children, the id of x is less than the id of any child and less
// than the id of any child's descendent.

/* HACK HACK HACK: this used to be static in `hpcfile_cstree_write_node',
   but multiple invocations of `hpcfile_cstree_write' made that problematic. */
static hpcfile_uint_t id = 0;

//***************************************************************************
// hpcfile_cstree_write()
//***************************************************************************

// See header file for documentation of public interface.
// Cf. 'HPC_CSTREE format details' above.
int
hpcfile_cstree_write(FILE* fs, void* tree, void* root,
		     hpcfile_uint_t num_metrics,
		     hpcfile_uint_t num_nodes,
                     hpcfile_uint_t epoch,
		     hpcfile_cstree_cb__get_data_fn_t get_data_fn,
		     hpcfile_cstree_cb__get_first_child_fn_t get_first_child_fn,
		     hpcfile_cstree_cb__get_sibling_fn_t get_sibling_fn)
{
  hpcfile_cstree_hdr_t fhdr;
  hpcfile_cstree_node_t fnode;
  int ret;

  if (!fs) { return HPCFILE_ERR; }
  
  // Write header
  hpcfile_cstree_hdr__init(&fhdr);
  fhdr.epoch = epoch;
  fhdr.num_nodes = num_nodes;
  if (hpcfile_cstree_hdr__fwrite(&fhdr, fs) != HPCFILE_OK) { 
    return HPCFILE_ERR; 
  }

  /* reset the node id */
  id = 0;

  // Write each node, beginning with root
  hpcfile_cstree_node__init(&fnode);
  fnode.data.num_metrics = num_metrics;
  fnode.data.metrics = malloc(num_metrics * sizeof(hpcfile_uint_t));
  ret = hpcfile_cstree_write_node(fs, tree, root, &fnode, 0, get_data_fn, 
				  get_first_child_fn, get_sibling_fn);
  free(fnode.data.metrics);
  
  return ret;
}

static int
hpcfile_cstree_write_node(FILE* fs, void* tree, void* node, 
			  hpcfile_cstree_node_t *scratch,
			  hpcfile_uint_t id_parent,
			  hpcfile_cstree_cb__get_data_fn_t get_data_fn,
			  hpcfile_cstree_cb__get_first_child_fn_t get_first_child_fn,
			  hpcfile_cstree_cb__get_sibling_fn_t get_sibling_fn)
{
  hpcfile_uint_t myid;
  void* first, *c;

  if (!node) { return HPCFILE_OK; }

  scratch->id = myid = id;
  scratch->id_parent = id_parent;
  get_data_fn(tree, node, &(scratch->data));

  if (hpcfile_cstree_node__fwrite(scratch, fs) != HPCFILE_OK) { 
    return HPCFILE_ERR; 
  }

  // Prepare next id -- assigned in the order that this func is called
  id++;
  
  // Write each child of node.  Works with either a circular or
  // non-circular structure
  first = c = get_first_child_fn(tree, node);
  while (c) {
    if (hpcfile_cstree_write_node(fs, tree, c, scratch, myid, get_data_fn, 
				  get_first_child_fn, get_sibling_fn) 
	!= HPCFILE_OK) {
      return HPCFILE_ERR;
    }

    c = get_sibling_fn(tree, c);
    if (c == first) { break; }
  }
  
  return HPCFILE_OK;
}

//***************************************************************************
// hpcfile_cstree_read()
//***************************************************************************

// See header file for documentation of public interface.
// Cf. 'HPC_CSTREE format details' above.
int
hpcfile_cstree_read(FILE* fs, void* tree, 
		    hpcfile_cstree_cb__create_node_fn_t create_node_fn,
		    hpcfile_cstree_cb__link_parent_fn_t link_parent_fn,
		    hpcfile_cb__alloc_fn_t alloc_fn,
		    hpcfile_cb__free_fn_t free_fn)
{
  hpcfile_cstree_hdr_t fhdr;
  hpcfile_cstree_node_t fnode;
  void* node, *parent;
  int i, ret = HPCFILE_ERR;
  
  // A vector storing created tree nodes.  The vector indices correspond to
  // the nodes' persistent ids.
  void** node_vec = NULL;

  if (!fs) { return HPCFILE_ERR; }
  
  // Read and sanity check header
  if (hpcfile_cstree_read_hdr(fs, &fhdr) != HPCFILE_OK) { 
    return HPCFILE_ERR; 
  }
  
  // Allocate space for 'node_vec'
  if (fhdr.num_nodes != 0) {
    node_vec = alloc_fn(sizeof(void*) * fhdr.num_nodes);
  }
  
  // Read each node, creating it and linking it to its parent 
  for (i = 0; i < fhdr.num_nodes; ++i) {

    if (hpcfile_cstree_node__fread(&fnode, fs) != HPCFILE_OK) { 
      goto cstree_read_cleanup; // HPCFILE_ERR
    }
    if (fnode.id_parent >= fhdr.num_nodes) { 
      goto cstree_read_cleanup; // HPCFILE_ERR
    }
    parent = node_vec[fnode.id_parent];

    // parent should already exist unless id_parent and id are equal
    if (!parent && fnode.id_parent != fnode.id) { 
      goto cstree_read_cleanup; // HPCFILE_ERR
    }

    // Create node and link to parent
    node = create_node_fn(tree, &fnode.data);
    node_vec[fnode.id] = node;
    
    if (parent) {
      link_parent_fn(tree, node, parent);
    } 
  }

  // Success! Note: We assume that it is possible for other data to
  // exist beyond this point in the stream; don't check for EOF.
  ret = HPCFILE_OK;
  
  // Close file and cleanup
 cstree_read_cleanup:
  if (node_vec) { free_fn(node_vec); }
  
  return ret;
}

//***************************************************************************
// hpcfile_cstree_convert_to_txt()
//***************************************************************************

// See header file for documentation of public interface.
int
hpcfile_cstree_convert_to_txt(FILE* infs, FILE* outfs)
{
  hpcfile_cstree_hdr_t fhdr;
  hpcfile_cstree_node_t fnode;
  int i;
  
  // Open file for reading; read and sanity check header
  if (hpcfile_cstree_read_hdr(infs, &fhdr) != HPCFILE_OK) {
    fputs("** Error reading header **\n", outfs);
    return HPCFILE_ERR;
  }
  
  // Print header
  hpcfile_cstree_hdr__fprint(&fhdr, outfs);
  fputs("\n", outfs);

  // Read and print each node
  for (i = 0; i < fhdr.num_nodes; ++i) {
    if (hpcfile_cstree_node__fread(&fnode, infs) != HPCFILE_OK) { 
      fprintf(outfs, "** Error reading node number %d **\n", i);
      return HPCFILE_ERR;
    }
    hpcfile_cstree_node__fprint(&fnode, outfs);
  }
  
  // Success! Note: We assume that it is possible for other data to
  // exist beyond this point in the stream; don't check for EOF.
  return HPCFILE_OK;
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
 
  if ((c = fgetc(fs)) == EOF) { return HPCFILE_ERR; }
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
hpcfile_cstree_id__fprint(hpcfile_cstree_id_t* x, FILE* fs)
{
  fprintf(fs, "{fileid: (magic: %s) (ver: %s) (end: %c)}\n", 
	  x->magic_str, x->version, x->endian);

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
  fputs("{cstree_hdr:\n", fs);

  hpcfile_cstree_id__fprint(&x->fid, fs);

  fprintf(fs, "(vma_sz: %u) (uint_sz: %u) (num_nodes: %lu)}\n", 
	  x->vma_sz, x->uint_sz, x->num_nodes);
  
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
  size_t sz;
  int i;

  sz = hpc_fread_le8(&x->ip, fs);
  if (sz != sizeof(x->ip)) { return HPCFILE_ERR; }

  sz = hpc_fread_le8(&x->sp, fs);
  if (sz != sizeof(x->sp)) { return HPCFILE_ERR; }

  for(i = 0; i<x->num_metrics; ++i) {
    sz = hpc_fread_le8(&x->metrics[i], fs);
    if (sz != sizeof(x->metrics[i])) { return HPCFILE_ERR; }
  }

  return HPCFILE_OK;
}

int 
hpcfile_cstree_nodedata__fwrite(hpcfile_cstree_nodedata_t* x, FILE* fs)
{
  size_t sz;
  int i;
  
  sz = hpc_fwrite_le8(&x->ip, fs);
  if (sz != sizeof(x->ip)) { return HPCFILE_ERR; }

  sz = hpc_fwrite_le8(&x->sp, fs);
  if (sz != sizeof(x->sp)) { return HPCFILE_ERR; }
  
  for(i = 0; i<x->num_metrics; ++i) {
    sz = hpc_fwrite_le8(&x->metrics[i], fs);
    if (sz != sizeof(x->metrics[i])) { return HPCFILE_ERR; }
  }

  return HPCFILE_OK;
}

int 
hpcfile_cstree_nodedata__fprint(hpcfile_cstree_nodedata_t* x, FILE* fs)
{
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

  sz = hpc_fread_le8(&x->id, fs);
  if (sz != sizeof(x->id)) { return HPCFILE_ERR; }
  
  sz = hpc_fread_le8(&x->id_parent, fs);
  if (sz != sizeof(x->id_parent)) { return HPCFILE_ERR; }
  
  if (hpcfile_cstree_nodedata__fread(&x->data, fs) != HPCFILE_OK) {
    return HPCFILE_ERR; 
  }
  
  return HPCFILE_OK;
}

int 
hpcfile_cstree_node__fwrite(hpcfile_cstree_node_t* x, FILE* fs)
{
  size_t sz;

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
hpcfile_cstree_node__fprint(hpcfile_cstree_node_t* x, FILE* fs)
{
  fputs("{node:\n", fs);

  hpcfile_cstree_nodedata__fprint(&x->data, fs);
  
  fprintf(fs, "(id: %lu) (id_parent: %lu)}\n", x->id, x->id_parent);
  
  return HPCFILE_OK;
}

//***************************************************************************

