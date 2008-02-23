// for each field above, define a td_get macro using this template:
//
// #define td_get_%FIELD%() ( csprof_get_thread_data()->%FIELD% )
//

#define td_get_id() ( csprof_get_thread_data()->id )
#define td_get_state() ( csprof_get_thread_data()->state )
#define td_get_bad_unwind() ( csprof_get_thread_data()->bad_unwind )
#define td_get_eventSet() ( csprof_get_thread_data()->eventSet )
#define td_get_handling_sample() ( csprof_get_thread_data()->handling_sample )
