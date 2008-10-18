/******************************************************************************
 * this is a draft of an interface that we can use to separate sampling 
 * sources from the profiler core. 
 *
 * each sampling source will define a method table.
 * sampling sources will register themselves to process arguments using
 * an init constructor. (that part of the interface isn't worked out yet.) 
 *
 * any sampling source selected with the command line arguments will 
 * register a method table with this interface. all active sampling sources 
 * registered will be invoked when an action on sampling sources 
 * (init, start, stop) is initiated.
 *****************************************************************************/

/******************************************************************************
 * type declarations 
 *****************************************************************************/
typedef enum {
  SAMPLING_INIT=0,
  SAMPLING_START=1,
  SAMPLING_STOP=2,
  SAMPLING_NMETHODS=3
} sampling_method;

typedef (*sampling_method_fn)(thread_data_t *td);

typedef struct{
  methods[SAMPLING_NMETHODS];
} sample_source_globals_t;



/******************************************************************************
 * local variables
 *****************************************************************************/
static sample_source_globals_t *ssg = NULL;



/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void call_sampling_methods(sampling_method method, thread_data_t *td);
static void call_sampling_method(sampling_source_globals_t *g, 
				 sampling_method method, thread_data_t *td);



/******************************************************************************
 * interface functions 
 *****************************************************************************/

void
csprof_add_sample_source(sample_source_globals_t *_ssgtd)
{
  ssgd->next = ssgd;
  ssgd = _ssgd;
}


void
csprof_start_sampling(thread_data_t *td)
{
  call_sampling_methods(SAMPLING_STOP, td);
}


void
csprof_stop_sampling(thread_data_t *td)
{
  call_sampling_methods(SAMPLING_START, td);
}


void
csprof_init_sampling()
{
  call_sampling_methods(SAMPLING_INIT, td);
}


/******************************************************************************
 * private operations
 *****************************************************************************/

static void
call_sampling_methods(sampling_method mid, thread_data_t *td)
{
  sampling_source_globals_t *g = ssg;
  for(; g; g = g->next) {
    call_sampling_method(g, mid, td);
  }
}


static void
call_sampling_method(sampling_source_globals_t *g, 
		     sampling_method mid, thread_data_t *td)
{
  (g->methods[mid])(td);
}


