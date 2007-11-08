
/* cache functions: accessors, resizers, etc. */

#ifndef CSPROF_TRAMPOLINE_BACKEND
/* cache_set: Updates the cached arrays that are designed to speed up
   tree insertions.  Adds tree node 'node' and instruction pointer 'ip' to
   the cached arrays at depth 'depth'.  Note that depth is 1 based and a
   depth of 1 refers to the last element of the array. */
static int
cache_set(csprof_cct_t* x, unsigned int depth, void* ip,
	  csprof_cct_node_t* node)
{
    unsigned int i;

    /* 1. Resize the arrays if necessary */
    if (depth > x->cache_len) {
        cache_resize(x, depth);
    }
   
    /* 2. Update cached arrays */
    i = x->cache_len - depth;
    x->cache_bt[i] = ip;
    x->cache_nodes[i] = node;
  
    return CSPROF_OK;
}

/* cache_set_depth: sets the cache depth to 'depth' */
static int 
cache_set_depth(csprof_cct_t* x, unsigned int depth)
{
    if (depth > x->cache_len) { 
        cache_resize(x, depth);
    }
    x->cache_top = x->cache_len - depth;
    return CSPROF_OK;
}

/* cache_resize: resizes the cache, ensuring that it is at least of
   size 'depth' */
static int 
cache_resize(csprof_cct_t* x, unsigned int depth)
{
    if (depth > x->cache_len) {
        // only grow larger
        void** old_bt                          = x->cache_bt;
        csprof_cct_node_t** old_nodes = x->cache_nodes;
        unsigned int old_len = x->cache_len, padding = 0;
    
        x->cache_len = x->cache_len * 2;
        if (depth > x->cache_len) { x->cache_len = depth; }
    
        x->cache_bt    = csprof_malloc(sizeof(void*) * x->cache_len);
        x->cache_nodes = csprof_malloc(sizeof(csprof_cct_node_t*) * 
                                       x->cache_len);
        DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION,
                   "resizing cct cache: %d -> %d length", old_len, x->cache_len);
    
        padding = x->cache_len - old_len;
        memcpy(x->cache_bt + padding, old_bt, sizeof(void*) * old_len);
        memcpy(x->cache_nodes + padding, old_nodes, sizeof(void*) * old_len);
        x->cache_top = x->cache_top + padding;
    
        /* no need to free */
    }
    return CSPROF_OK;
}

/* cache_get_bt: Gets the backtrace cache entry at depth 'depth'.  
   Note that 'depth' is 1 based and that 1 refers to the bottom of the
   stack (the root of the tree). */
static void* 
cache_get_bt(csprof_cct_t* x, unsigned int depth)
{
    unsigned int i = x->cache_len - depth;
    if (depth > x->cache_len || i < x->cache_top) { return NULL; }
    return x->cache_bt[i];
}

/* cache_get_node: Gets the cache node at depth 'depth'. 
   Note that 'depth' is 1 based and that 1 refers to the bottom of the
   stack (the root of the tree). */
static csprof_cct_node_t* 
cache_get_node(csprof_cct_t* x, unsigned int depth)
{
    unsigned int i = x->cache_len - depth;
    if (depth > x->cache_len || i < x->cache_top) { return NULL; }
    return x->cache_nodes[i];  
}

#endif
