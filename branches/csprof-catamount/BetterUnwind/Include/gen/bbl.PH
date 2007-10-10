//Groups: @ingroup\s+(API_REF|IMG_BASIC_API|INS_BASIC_API|INS_INST_API|INS_BASIC_API_GEN_IA32|INS_BASIC_API_IA32|INS_BASIC_API_IPF|INS_MOD_API_GEN_IA32|SEC_BASIC_API|RTN_BASIC_API|REG_BASIC_API|REG_CPU_GENERIC|REG_CPU_IPF|REG_CPU_IA32|TRACE_BASIC_API|BBL_BASIC_API|SYM_BASIC_API|MISC_PRINT|MISC_PARSE|KNOB_API|KNOB_BASIC|KNOB_PRINT|LOCK|PIN_CONTROL|PROTO_API|PIN_THREAD_API|DEBUG_API|TYPE_BASE|INSTLIB|ALARM|ROGUE|ROGUE_CFG|ROGUE_CFG_BASIC_API|ROGUE_EDG_BASIC_API|ROGUE_BBL_BASIC_API|ROGUE_INS_BASIC_API|ROGUE_TRACE_BASIC_API|ROGUE_CONTROL_BASIC_API|CODECACHE_API|SUPERPIN_API|OPTIMIZATION_API|MISC|CHECKPOINTING|PROTO|INST_ARGS|PIN_THREAD_PRIVATE|EDG_BASIC|BBL_BASIC|ROGUE_BASIC_API|CODECACHE_NYI_API|INS_BASIC_API_GEN_IPF|MESSAGE_TYPE|MESSAGE_BASIC|MISC_BASIC|ITC_INST_API|CHECKPOINTING_PRIVATE|UNDOCUMENTED_PIN_API|DEPRECATED_PIN_API
/* PIN API */

/* THIS FILE IS AUTOMAGICALLY GENERATED - DO NOT CHANGE DIRECTLY*/


typedef enum 
{
    BBL_TYPE_INVALID,       ///< illegal type, to indicate unknown type use BBL_TYPE_UNKNOWN instead
    BBL_TYPE_UBREAK,        ///< break (no go)
    BBL_TYPE_CBREAK,        ///< conditional break
    BBL_TYPE_STOP,          ///< stop (no go)
    BBL_TYPE_UCALL_UNKNOWN, ///< (indirect) call to unknown target 
    BBL_TYPE_UCALL_FUN,     ///< call to known target 
    BBL_TYPE_UCALL_OS,      ///< os call
    BBL_TYPE_CCALL_OS,      ///< conditional os call
    BBL_TYPE_CCALL_UNKNOWN, ///< conditonal (indirect) call to unknown target 
    BBL_TYPE_CCALL_FUN,     ///< conditonal call to known target
    BBL_TYPE_NORMAL,        ///< fallthrough bbl
    BBL_TYPE_CBRANCH,       ///< conditional branch
    BBL_TYPE_UBRANCH,       ///< unconditional branch
    BBL_TYPE_COUNTED,       ///< conditional branch (counted)   
    BBL_TYPE_SWPIPELINED,   ///< conditional branch (counted,register rotation)
    BBL_TYPE_CHECK,         ///< check
    BBL_TYPE_PREFETCH,      ///< prefetch
    BBL_TYPE_UJUMP,         ///< jump 
    BBL_TYPE_UJUMP_DYNLINK, ///< jump into the dynamic linker (in .plt) 
    BBL_TYPE_UJUMP_THUNK,   ///< call of dynamically linked code
    BBL_TYPE_UJUMP_TRAMPOLINE, ///< jump through a trampoline
    BBL_TYPE_UBRANCH_TRAMPOLINE, ///< jump through a trampoline
    BBL_TYPE_CJUMP,         ///< conditional jump
    BBL_TYPE_CJUMP_SWITCH,  ///< conditional jump 
    BBL_TYPE_STUB,          ///< jump bbl that implements a shared lib stub
    BBL_TYPE_URETURN,       ///< return 
    BBL_TYPE_CRETURN,       ///< conditional return 
    BBL_TYPE_DATA,          ///< data  -- NB: data entries must be contiguous. see BBL_IsData
    BBL_TYPE_DATA_IADDR,    ///< data -- pin IADDR for ip relative branch on em64t
    BBL_TYPE_DATA_ARGBLOCK, ///< data -- pin argument block for VM
    BBL_TYPE_DATA_SWITCH1,  ///< data 
    BBL_TYPE_DATA_LAST,     ///< data 
    BBL_TYPE_CONTAINER,     ///< initial container
    BBL_TYPE_UNKNOWN,       ///< unknown type
    BBL_TYPE_VMSVC,         ///< vm service request
    BBL_TYPE_LOOKUP,        ///< indirect hash table lookup code
    BBL_TYPE_EMU_FT,        ///< invoke pin emulator on instruction with a fall through path
    BBL_TYPE_LAST
}BBL_TYPE;

                                                                  /* DO NOT EDIT */
typedef enum 
{
    BBL_STATE_INVALID,
    BBL_STATE_LAST
}BBL_STATE;

                                                                  /* DO NOT EDIT */
typedef struct
{
    ADDRINT pos;
}BBL_STRUCT_MAP;

                                                                  /* DO NOT EDIT */
extern BBL BBL_Clone(BBL bbl);

                                                                  /* DO NOT EDIT */
extern VOID BBL_InitOriginal(BBL bbl);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_HasRegsym(BBL bbl);

                                                                  /* DO NOT EDIT */
extern UINT32 BBL_NumAttrCrosslink(BBL bbl);

                                                                  /* DO NOT EDIT */
extern UINT32 BBL_NumIns(BBL bbl);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_IsImageEntryPoint(BBL bbl);

                                                                  /* DO NOT EDIT */
extern VOID  BBL_SetImageEntryPoint(BBL bbl);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_ContainsCall(BBL bbl);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_ContainsUnconditionalCall(BBL bbl);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_ContainsConditionalCall(BBL bbl);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_ContainsIndirectCall(BBL bbl);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_ContainsConditionalControlTransfer(BBL bbl);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_ContainsDirectControlTransfer(BBL bbl);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_ContainsIndirectControlTransfer(BBL bbl);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_IsReturnSite(BBL bbl);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_HasUnmodeledSuccEdges(BBL bbl);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_ContainsUnresolvedIndirectJump(BBL bbl);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_ContainsReturn(BBL bbl);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_HasUnmodeledPredEdges(BBL bbl, BOOL switch_ok);

                                                                  /* DO NOT EDIT */
extern BBL BBL_UniquePredecessor(BBL bbl);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_CanBeEntryPoint(BBL bbl, BOOL switch_ok);

                                                                  /* DO NOT EDIT */
extern VOID BBL_TypeSet(BBL bbl, BBL_TYPE type);

                                                                  /* DO NOT EDIT */
extern CHUNK BBL_DataChunk(BBL bbl);

                                                                  /* DO NOT EDIT */

