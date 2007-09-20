//Groups: @ingroup\s+(API_REF|IMG_BASIC_API|INS_BASIC_API|INS_INST_API|INS_BASIC_API_GEN_IA32|INS_BASIC_API_IA32|INS_BASIC_API_IPF|INS_MOD_API_GEN_IA32|SEC_BASIC_API|RTN_BASIC_API|REG_BASIC_API|REG_CPU_GENERIC|REG_CPU_IPF|REG_CPU_IA32|TRACE_BASIC_API|BBL_BASIC_API|SYM_BASIC_API|MISC_PRINT|MISC_PARSE|KNOB_API|KNOB_BASIC|KNOB_PRINT|LOCK|PIN_CONTROL|PROTO_API|PIN_THREAD_API|DEBUG_API|TYPE_BASE|INSTLIB|ALARM|ROGUE|ROGUE_CFG|ROGUE_CFG_BASIC_API|ROGUE_EDG_BASIC_API|ROGUE_BBL_BASIC_API|ROGUE_INS_BASIC_API|ROGUE_TRACE_BASIC_API|ROGUE_CONTROL_BASIC_API|CODECACHE_API|SUPERPIN_API|OPTIMIZATION_API|MISC|CHECKPOINTING|PROTO|INST_ARGS|PIN_THREAD_PRIVATE|EDG_BASIC|BBL_BASIC|ROGUE_BASIC_API|CODECACHE_NYI_API|INS_BASIC_API_GEN_IPF|MESSAGE_TYPE|MESSAGE_BASIC|MISC_BASIC|ITC_INST_API|CHECKPOINTING_PRIVATE|UNDOCUMENTED_PIN_API|DEPRECATED_PIN_API
/* PIN API */

/* THIS FILE IS AUTOMAGICALLY GENERATED - DO NOT CHANGE DIRECTLY*/


typedef VOID (*SPSLICE_BEGIN_CALLBACK)(INT32 sliceNum, VOID *v);

                                                                  /* DO NOT EDIT */
typedef VOID (*SPSLICE_END_CALLBACK)(INT32 sliceNum, VOID *v);

                                                                  /* DO NOT EDIT */
typedef VOID (*SPSLICE_TOOLRESET_CALLBACK)(UINT32 sliceNum);

                                                                  /* DO NOT EDIT */
extern VOID SP_AddSliceBeginFunction(SPSLICE_BEGIN_CALLBACK fun, VOID *val);

                                                                  /* DO NOT EDIT */
extern VOID SP_AddSliceEndFunction(SPSLICE_END_CALLBACK fun, VOID *val);

                                                                  /* DO NOT EDIT */
extern VOID SP_EndSlice();

                                                                  /* DO NOT EDIT */
extern BOOL SP_Init(SPSLICE_TOOLRESET_CALLBACK fun);

                                                                  /* DO NOT EDIT */
extern VOID *SP_CreateSharedArea(VOID *localData, UINT32 size, UINT32 autoMerge);

                                                                  /* DO NOT EDIT */

