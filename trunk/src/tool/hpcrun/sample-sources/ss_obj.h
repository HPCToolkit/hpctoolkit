#undef _TPx
#undef _T3
#undef _make_id
#undef _st
#undef _st1

#define _TPx(a,b,c) a ## b ## c
#define _T3(a, b, c) _TPx(a, b, c)
#define _make_id(tpl) _T3 tpl

#define _st(n) # n
#define _st1(n) _st(n)

#undef obj_name
#undef ss_str
#undef reg_fn_name

#define obj_name() _make_id( (_,ss_name,_obj))
#define ss_str()  _st1(ss_name) 
#define reg_fn_name _make_id((,ss_name,_obj_reg))

sample_source_t obj_name() = {
  // common methods

  .add_event     = hpcrun_ss_add_event,
  .store_event   = hpcrun_ss_store_event,
  .store_metric_id = hpcrun_ss_store_metric_id,
  .get_event_str = hpcrun_ss_get_event_str,

  // specific methods

  .init = init,
  .start = start,
  .stop  = stop,
  .shutdown = shutdown,
  .supports_event = supports_event,
  .process_event_list = process_event_list,
  .gen_event_set = gen_event_set,
  .display_events = display_events,

  // data
  .evl = {
    .evl_spec = {[0] = '\0'},
    .nevents = 0
  },
  .evset_idx = -1,
  .name = ss_str(),
  .cls  = ss_cls,
  .state = UNINIT
};


/******************************************************************************
 * constructor 
 *****************************************************************************/

static void reg_fn_name(void) __attribute__ ((constructor));

static void
reg_fn_name(void)
{
  hpcrun_ss_register(&obj_name());
}

#undef obj_name
#undef ss_str
#undef reg_fn_name

#undef _TPx
#undef _T3
#undef _make_id
#undef _st
#undef _st1
