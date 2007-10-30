//Groups: @ingroup\s+(API_REF|IMG_BASIC_API|INS_BASIC_API|INS_INST_API|INS_BASIC_API_GEN_IA32|INS_BASIC_API_IA32|INS_BASIC_API_IPF|INS_MOD_API_GEN_IA32|SEC_BASIC_API|RTN_BASIC_API|REG_BASIC_API|REG_CPU_GENERIC|REG_CPU_IPF|REG_CPU_IA32|TRACE_BASIC_API|BBL_BASIC_API|SYM_BASIC_API|MISC_PRINT|MISC_PARSE|KNOB_API|KNOB_BASIC|KNOB_PRINT|LOCK|PIN_CONTROL|PROTO_API|PIN_THREAD_API|DEBUG_API|TYPE_BASE|INSTLIB|ALARM|ROGUE|ROGUE_CFG|ROGUE_CFG_BASIC_API|ROGUE_EDG_BASIC_API|ROGUE_BBL_BASIC_API|ROGUE_INS_BASIC_API|ROGUE_TRACE_BASIC_API|ROGUE_CONTROL_BASIC_API|CODECACHE_API|SUPERPIN_API|OPTIMIZATION_API|MISC|CHECKPOINTING|PROTO|INST_ARGS|PIN_THREAD_PRIVATE|EDG_BASIC|BBL_BASIC|ROGUE_BASIC_API|CODECACHE_NYI_API|INS_BASIC_API_GEN_IPF|MESSAGE_TYPE|MESSAGE_BASIC|MISC_BASIC|ITC_INST_API|CHECKPOINTING_PRIVATE|UNDOCUMENTED_PIN_API|DEPRECATED_PIN_API
/* PIN API */

/* THIS FILE IS AUTOMAGICALLY GENERATED - DO NOT CHANGE DIRECTLY*/


typedef VOID (*TRACEGENCODE_CALLBACK)(TRACE trace, VOID *v);

                                                                  /* DO NOT EDIT */
extern BBL TRACE_AddInlineReturnEdg(TRACE trace);

                                                                  /* DO NOT EDIT */
extern BBL TRACE_AddInlineCallEdg(TRACE trace);

                                                                  /* DO NOT EDIT */
extern BBL TRACE_AddBranchEdg(TRACE trace);

                                                                  /* DO NOT EDIT */
extern BBL TRACE_AddFallthroughEdg(TRACE trace);

                                                                  /* DO NOT EDIT */
extern VOID TRACE_StraightenControlFlow(TRACE trace);

                                                                  /* DO NOT EDIT */
extern VOID PIN_RemoveAllProbes();

                                                                  /* DO NOT EDIT */
extern VOID PIN_InsertProbe(ADDRINT src_addr, ADDRINT dst_addr);

                                                                  /* DO NOT EDIT */
extern VOID PIN_RemoveProbe(ADDRINT address);

                                                                  /* DO NOT EDIT */
extern ADDRINT TRACE_GenerateCode(TRACE trace);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsPinXfer(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsNativeXfer(INS ins);

                                                                  /* DO NOT EDIT */
extern VOID INS_SetNativeXfer(INS ins);

                                                                  /* DO NOT EDIT */
extern VOID TRACE_ExitsXferToNative(TRACE trace);

                                                                  /* DO NOT EDIT */
extern VOID INS_SetPinXfer(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsNativeCall(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsXlateCall(INS ins);

                                                                  /* DO NOT EDIT */
extern VOID INS_SetXlateCall(INS ins);

                                                                  /* DO NOT EDIT */
extern VOID INS_SetNativeCall(INS ins);

                                                                  /* DO NOT EDIT */
extern VOID INS_RedirectControlFlowToAddress(INS ins, ADDRINT target_addr);

                                                                  /* DO NOT EDIT */
extern BOOL PIN_IsProbeMode();

                                                                  /* DO NOT EDIT */
extern BOOL RTN_IsSafeForProbedReplacement(RTN rtn);

                                                                  /* DO NOT EDIT */
extern AFUNPTR RTN_ReplaceSignatureProbed(RTN orgRtn, AFUNPTR replacementFunptr, ...);

                                                                  /* DO NOT EDIT */
extern AFUNPTR RTN_InsertProbe(RTN rtn, AFUNPTR probe);

                                                                  /* DO NOT EDIT */
extern PROTO PROTO_Allocate( PARG_T returnArg,
                                CALLINGSTD_TYPE cstype, const char * name, ... );

                                                                  /* DO NOT EDIT */
extern VOID PROTO_Free( PROTO proto );

                                                                  /* DO NOT EDIT */

