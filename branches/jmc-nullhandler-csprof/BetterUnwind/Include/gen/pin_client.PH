//Groups: @ingroup\s+(API_REF|IMG_BASIC_API|INS_BASIC_API|INS_INST_API|INS_BASIC_API_GEN_IA32|INS_BASIC_API_IA32|INS_BASIC_API_IPF|INS_MOD_API_GEN_IA32|SEC_BASIC_API|RTN_BASIC_API|REG_BASIC_API|REG_CPU_GENERIC|REG_CPU_IPF|REG_CPU_IA32|TRACE_BASIC_API|BBL_BASIC_API|SYM_BASIC_API|MISC_PRINT|MISC_PARSE|KNOB_API|KNOB_BASIC|KNOB_PRINT|LOCK|PIN_CONTROL|PROTO_API|PIN_THREAD_API|DEBUG_API|TYPE_BASE|INSTLIB|ALARM|ROGUE|ROGUE_CFG|ROGUE_CFG_BASIC_API|ROGUE_EDG_BASIC_API|ROGUE_BBL_BASIC_API|ROGUE_INS_BASIC_API|ROGUE_TRACE_BASIC_API|ROGUE_CONTROL_BASIC_API|CODECACHE_API|SUPERPIN_API|OPTIMIZATION_API|MISC|CHECKPOINTING|PROTO|INST_ARGS|PIN_THREAD_PRIVATE|EDG_BASIC|BBL_BASIC|ROGUE_BASIC_API|CODECACHE_NYI_API|INS_BASIC_API_GEN_IPF|MESSAGE_TYPE|MESSAGE_BASIC|MISC_BASIC|ITC_INST_API|CHECKPOINTING_PRIVATE|UNDOCUMENTED_PIN_API|DEPRECATED_PIN_API
/* PIN API */

/* THIS FILE IS AUTOMAGICALLY GENERATED - DO NOT CHANGE DIRECTLY*/


/* DO NOT EDIT */

#define EnterPinClientSlaveMode() CheckPinClientLock(__FUNCTION__)
#define ExitPinClientSlaveMode() CheckPinClientLock(__FUNCTION__)
#define LOG_IMAGE(message)          QMESSAGE(MessageTypeLogImage, string("Image:  ")     + message)
#define LOG_SYM(message)            QMESSAGE(MessageTypeLogSym, string("Symbols:  ")     + message)
#define LOG_PROBE(message)          QMESSAGE(MessageTypeLogProbe,string("Probe:  ")      +  message)

class TRACE_CLASS;

                                                                  /* DO NOT EDIT */
typedef TRACE_CLASS * TRACE;

                                                                  /* DO NOT EDIT */
typedef VOID (*REMOVE_INSTRUMENTATION_CALLBACK)(VOID *v);

                                                                  /* DO NOT EDIT */
typedef VOID (*DETACH_CALLBACK)(VOID *v);

                                                                  /* DO NOT EDIT */
typedef VOID (*FINI_CALLBACK)(INT32 code, VOID *v);

                                                                  /* DO NOT EDIT */
typedef VOID (*FORK_CALLBACK)(THREADID threadid, const CONTEXT *ctxt, VOID *v);

                                                                  /* DO NOT EDIT */
typedef VOID (*THREAD_START_CALLBACK)(THREADID threadIndex, CONTEXT *ctxt, INT32 flags, VOID *v);

                                                                  /* DO NOT EDIT */
typedef VOID (*THREAD_FINI_CALLBACK)(THREADID threadIndex, const CONTEXT *ctxt, INT32 code, VOID *v);

                                                                  /* DO NOT EDIT */
typedef VOID (*CONTEXT_CHANGE_CALLBACK)(THREADID threadIndex, CONTEXT_CHANGE_REASON reason,
    const CONTEXT *from, CONTEXT *to, INT32 info, VOID *v);

                                                                  /* DO NOT EDIT */
typedef BOOL (*SIGNAL_INTERCEPT_CALLBACK)(THREADID threadIndex, INT32 sig, CONTEXT *ctxt, BOOL hasHndlr, VOID *v);

                                                                  /* DO NOT EDIT */
typedef VOID (*INS_INSTRUMENT_CALLBACK)(INS ins, VOID *v);

                                                                  /* DO NOT EDIT */
typedef VOID (*TRACE_INSTRUMENT_CALLBACK)(TRACE trace, VOID *v);

                                                                  /* DO NOT EDIT */
typedef VOID (*RTN_INSTRUMENT_CALLBACK)(RTN rtn, VOID *v);

                                                                  /* DO NOT EDIT */
extern VOID PIN_LockClient();

                                                                  /* DO NOT EDIT */
extern VOID PIN_UnlockClient();

                                                                  /* DO NOT EDIT */
extern VOID PIN_LockClient();

                                                                  /* DO NOT EDIT */
extern VOID PIN_UnlockClient();

                                                                  /* DO NOT EDIT */
extern VOID RTN_ReplaceWithUninstrumentedRoutine(RTN replacedRtn, AFUNPTR replacementFun);

                                                                  /* DO NOT EDIT */
extern AFUNPTR RTN_Replace(RTN replacedRtn, AFUNPTR replacementFun);

                                                                  /* DO NOT EDIT */
extern AFUNPTR RTN_ReplaceProbed(RTN replacedRtn, AFUNPTR replacementFun);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsAddedForFunctionReplacement(INS ins);

                                                                  /* DO NOT EDIT */
extern VOID PIN_RegisterItcAuxCallBackPushFun(AFUNPTR callBackPushFun);

                                                                  /* DO NOT EDIT */
extern VOID RTN_ComplexReplaceWithUninstrumentedRoutine(RTN replacedRtn, AFUNPTR replacementFun);

                                                                  /* DO NOT EDIT */
extern ADDRINT PIN_FindAlternateAppStack();

                                                                  /* DO NOT EDIT */
extern VOID TRACE_AddInstrumentFunction(TRACE_INSTRUMENT_CALLBACK fun, VOID *val);

                                                                  /* DO NOT EDIT */
extern VOID CODECACHE_AddTraceInsertedFunction(TRACE_INSTRUMENT_CALLBACK fun, VOID *val);

                                                                  /* DO NOT EDIT */
extern VOID INS_AddInstrumentFunction(INS_INSTRUMENT_CALLBACK fun, VOID *val);

                                                                  /* DO NOT EDIT */
extern VOID PIN_AddFiniFunction(FINI_CALLBACK fun, VOID *val);

                                                                  /* DO NOT EDIT */
extern VOID PIN_AddDetachFunction(DETACH_CALLBACK fun, VOID *val);

                                                                  /* DO NOT EDIT */
extern VOID PIN_AddThreadStartFunction(THREAD_START_CALLBACK fun, VOID *val);

                                                                  /* DO NOT EDIT */
extern VOID PIN_AddThreadFiniFunction(THREAD_FINI_CALLBACK fun, VOID *val);

                                                                  /* DO NOT EDIT */
extern VOID PIN_AddContextChangeFunction(CONTEXT_CHANGE_CALLBACK fun, VOID *val);

                                                                  /* DO NOT EDIT */
extern BOOL PIN_AddSignalInterceptFunction(INT32 sig, SIGNAL_INTERCEPT_CALLBACK fun, VOID *val);

                                                                  /* DO NOT EDIT */
enum FPOINT
{
    FPOINT_BEFORE,          ///< Call-back in parent, just before fork
    FPOINT_AFTER_IN_PARENT, ///< Call-back in parent, immediately after fork
    FPOINT_AFTER_IN_CHILD   ///< Call-back in child, immediately after fork
};

                                                                  /* DO NOT EDIT */
extern BOOL PIN_AddForkFunction(FPOINT point, FORK_CALLBACK fun, VOID *val);

                                                                  /* DO NOT EDIT */
extern VOID PIN_RemoveInstrumentation();

                                                                  /* DO NOT EDIT */
extern VOID PIN_RemoveFiniFunctions();

                                                                  /* DO NOT EDIT */
extern VOID PIN_Detach();

                                                                  /* DO NOT EDIT */
extern VOID INS_InsertPredicatedCall(INS ins, IPOINT ipoint, AFUNPTR funptr, ...);

                                                                  /* DO NOT EDIT */
extern VOID INS_InsertCall(INS ins, IPOINT action, AFUNPTR funptr, ...);

                                                                  /* DO NOT EDIT */
extern VOID INS_InsertIfCall(INS ins, IPOINT action, AFUNPTR funptr, ...);

                                                                  /* DO NOT EDIT */
extern VOID INS_InsertThenCall(INS ins, IPOINT action, AFUNPTR funptr, ...);

                                                                  /* DO NOT EDIT */
extern VOID INS_InsertIfPredicatedCall(INS ins, IPOINT action, AFUNPTR funptr, ...);

                                                                  /* DO NOT EDIT */
extern VOID INS_InsertThenPredicatedCall(INS ins, IPOINT action, AFUNPTR funptr, ...);

                                                                  /* DO NOT EDIT */
extern VOID PIN_StartProgram();

                                                                  /* DO NOT EDIT */
extern VOID PIN_StartProgramProbed();

                                                                  /* DO NOT EDIT */
extern MESSAGE_TYPE MessageTypeLogProbe;

                                                                  /* DO NOT EDIT */
extern BOOL ParseCommandLine(int xargc, CHAR **xargv);

                                                                  /* DO NOT EDIT */
extern BOOL PIN_Init(INT32 argc, CHAR ** argv);

                                                                  /* DO NOT EDIT */
extern TRACE TRACE_Allocate(ADDRINT addr);

                                                                  /* DO NOT EDIT */
extern TRACE TRACE_AllocateForProbe(PROBE_TYPE probeType, ADDRINT addr);

                                                                  /* DO NOT EDIT */
extern VOID TRACE_Deallocate(TRACE trace);

                                                                  /* DO NOT EDIT */
extern VOID TRACE_InsertCall(TRACE trace, IPOINT action, AFUNPTR funptr, ...);

                                                                  /* DO NOT EDIT */
extern VOID TRACE_InsertIfCall(TRACE trace, IPOINT action, AFUNPTR funptr, ...);

                                                                  /* DO NOT EDIT */
extern VOID TRACE_InsertThenCall(TRACE trace, IPOINT action, AFUNPTR funptr, ...);

                                                                  /* DO NOT EDIT */
extern BBL TRACE_BblHead(TRACE trace);

                                                                  /* DO NOT EDIT */
extern BBL TRACE_BblTail(TRACE trace);

                                                                  /* DO NOT EDIT */
extern BOOL TRACE_Original(TRACE trace);

                                                                  /* DO NOT EDIT */
extern ADDRINT TRACE_Address(TRACE trace);

                                                                  /* DO NOT EDIT */
extern USIZE TRACE_Size(TRACE trace);

                                                                  /* DO NOT EDIT */
extern RTN TRACE_Rtn(TRACE trace);

                                                                  /* DO NOT EDIT */
extern BOOL TRACE_HasFallThrough(TRACE trace);

                                                                  /* DO NOT EDIT */
extern UINT32 TRACE_NumBbl(TRACE trace);

                                                                  /* DO NOT EDIT */
extern UINT32 TRACE_NumIns(TRACE trace);

                                                                  /* DO NOT EDIT */
extern VOID BBL_InsertCall(BBL bbl, IPOINT action, AFUNPTR funptr, ...);

                                                                  /* DO NOT EDIT */
extern VOID BBL_InsertIfCall(BBL bbl, IPOINT action, AFUNPTR funptr, ...);

                                                                  /* DO NOT EDIT */
extern VOID BBL_InsertThenCall(BBL bbl, IPOINT action, AFUNPTR funptr, ...);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_HasFallThrough(BBL bbl);

                                                                  /* DO NOT EDIT */
extern VOID PIN_SaveCheckpoint(CHECKPOINT * chkptFrom, CHECKPOINT * chkptTo);

                                                                  /* DO NOT EDIT */
extern VOID PIN_Resume(CHECKPOINT * chkpt);

                                                                  /* DO NOT EDIT */
extern VOID PIN_SetContextReg(CONTEXT * ctxt, const REG reg, const ADDRINT val);

                                                                  /* DO NOT EDIT */
extern ADDRINT PIN_GetContextReg(const CONTEXT * ctxt, const REG reg);

                                                                  /* DO NOT EDIT */
extern VOID PIN_SetContextFPState(CONTEXT * ctxt, VOID * fpstate);

                                                                  /* DO NOT EDIT */
extern VOID PIN_GetContextFPState(const CONTEXT * ctxt, VOID * fpstate);

                                                                  /* DO NOT EDIT */
extern VOID PIN_SaveContext(const CONTEXT * ctxtFrom, CONTEXT * ctxtTo);

                                                                  /* DO NOT EDIT */
extern VOID PIN_ExecuteAt(const CONTEXT * ctxt);

                                                                  /* DO NOT EDIT */
extern BOOL INS_ChangeMemOpToBaseRegisterAddressMode(INS ins, MEMORY_TYPE mtype, REG newBase);

                                                                  /* DO NOT EDIT */
extern BOOL INS_RewriteMemoryAddressingToBaseRegisterOnly(INS ins, 
                                                             MEMORY_TYPE mtype,
                                                             REG newBase);

                                                                  /* DO NOT EDIT */
extern VOID GetVmLock();

                                                                  /* DO NOT EDIT */
extern VOID ReleaseVmLock();

                                                                  /* DO NOT EDIT */
extern CHAR * PIN_VmFullPath();

                                                                  /* DO NOT EDIT */
extern OS_THREAD_ID PIN_GetTid();

                                                                  /* DO NOT EDIT */
extern THREADID PIN_ThreadId();

                                                                  /* DO NOT EDIT */
extern OS_THREAD_ID PIN_GetParentTid();

                                                                  /* DO NOT EDIT */
extern VOID INS_Delete(INS ins);

                                                                  /* DO NOT EDIT */
extern VOID IARGLIST_AddArguments(IARGLIST args,  ...);

                                                                  /* DO NOT EDIT */
extern IARGLIST IARGLIST_Alloc();

                                                                  /* DO NOT EDIT */
extern VOID IARGLIST_Free(IARGLIST args);

                                                                  /* DO NOT EDIT */
extern size_t PIN_SafeCopy(VOID * dst, const VOID * src, size_t size);

                                                                  /* DO NOT EDIT */

