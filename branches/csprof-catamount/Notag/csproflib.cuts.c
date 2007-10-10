#if defined(CSPROF_THREADS)
static void
csprof_duplicate_thread_state_reference(csprof_state_t *newstate)
{
    /* unfortunately, thread state is kept in *two* places: the
       thread-local data variable and the `all_threads' list.
       ugh ugh ugh.  go through and try to fix things (hopefully
       this should not be happening very often, so we're willing
       to pay the cost.  if this turns out to be problematic,
       we'll have to provide another layer of indirection so that
       threads have one state throughout their lifetime, with
       possibly several different data trees). */
    {
        pthread_t me = pthread_self();
        csprof_list_node_t *runner;

        runner = all_threads.head;

        for(; runner != NULL; runner = runner->next) {
            pthread_t other = (pthread_t)runner->ip;

            if(pthread_equal(me, other)) {
                runner->node = newstate;
                break;
            }
        }

        /* if we get here...oh well.  FIXME: might want to add an
           assert or something. */
    }
}
#endif

#ifdef CSPROF_THREADS
/* The primary reason for this function is that not all applications are
   nice enough to shut down their threads with pthread_exit, preferring
   instead to let application termination tear down the threads.  We
   rely on having pthread_exit called so that some of our cleanup code
   can be run.

   Since we can't rely on pthread_exit being called, we do the bare
   minimum in our pthread_exit and defer the flushing of profile data
   to disk until this point.  This simplifies a lot of things. */
void csprof_atexit_handler(){

  csprof_list_node_t *runner;

  MSG(CSPROF_MSG_DATAFILE, "ATEXIT: Dumping thread states");

  for(runner = all_threads.head; runner; runner = runner->next) {
    MSG(1,"checking thread runner = %lx",runner);
    if(runner->sp == CSPROF_PTHREAD_LIVE) {
      csprof_state_t *state = (csprof_state_t *)runner->node;

      MSG(CSPROF_MSG_DATAFILE, "Exit Dumping state %#lx", state);

      csprof_write_profile_data(state);

      /* mark this as being written out */
      runner->sp = CSPROF_PTHREAD_DEAD;
    }
  }
}
#endif


