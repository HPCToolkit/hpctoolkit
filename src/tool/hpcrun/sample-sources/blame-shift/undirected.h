#ifndef __undirected_h__
#define __undirected_h__

/******************************************************************************
 * types 
 *****************************************************************************/

typedef int *(*undirected_idle_cnt_ptr_fn)(void);
typedef _Bool (*undirected_predicate_fn)(void);


typedef struct undirected_blame_info_t {
  long active_worker_count;
  long total_worker_count;

  undirected_idle_cnt_ptr_fn get_idle_count_ptr;
  undirected_predicate_fn participates;
  undirected_predicate_fn working;

  int work_metric_id;
  int idle_metric_id;

  int levels_to_skip;

} undirected_blame_info_t;



/******************************************************************************
 * interface operations
 *****************************************************************************/

void undirected_blame_thread_start(undirected_blame_info_t *bi);

void undirected_blame_thread_end(undirected_blame_info_t *bi);

void undirected_blame_idle_begin(undirected_blame_info_t *bi);

void undirected_blame_idle_end(undirected_blame_info_t *bi);

void undirected_blame_sample(void *arg, int metric_id, 
			     cct_node_t *node, int metric_incr);


#endif // __undirected_h__
