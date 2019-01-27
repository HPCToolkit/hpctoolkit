/******************************************************************************
 *  * macros
 *   *****************************************************************************/

#define ompt_event_may_occur(r) \
  ((r == ompt_set_sometimes) | (r ==  ompt_set_sometimes_paired) || (r ==  ompt_set_always))
