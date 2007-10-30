//Groups: @ingroup\s+(API_REF|IMG_BASIC_API|INS_BASIC_API|INS_INST_API|INS_BASIC_API_GEN_IA32|INS_BASIC_API_IA32|INS_BASIC_API_IPF|INS_MOD_API_GEN_IA32|SEC_BASIC_API|RTN_BASIC_API|REG_BASIC_API|REG_CPU_GENERIC|REG_CPU_IPF|REG_CPU_IA32|TRACE_BASIC_API|BBL_BASIC_API|SYM_BASIC_API|MISC_PRINT|MISC_PARSE|KNOB_API|KNOB_BASIC|KNOB_PRINT|LOCK|PIN_CONTROL|PROTO_API|PIN_THREAD_API|DEBUG_API|TYPE_BASE|INSTLIB|ALARM|ROGUE|ROGUE_CFG|ROGUE_CFG_BASIC_API|ROGUE_EDG_BASIC_API|ROGUE_BBL_BASIC_API|ROGUE_INS_BASIC_API|ROGUE_TRACE_BASIC_API|ROGUE_CONTROL_BASIC_API|CODECACHE_API|SUPERPIN_API|OPTIMIZATION_API|MISC|CHECKPOINTING|PROTO|INST_ARGS|PIN_THREAD_PRIVATE|EDG_BASIC|BBL_BASIC|ROGUE_BASIC_API|CODECACHE_NYI_API|INS_BASIC_API_GEN_IPF|MESSAGE_TYPE|MESSAGE_BASIC|MISC_BASIC|ITC_INST_API|CHECKPOINTING_PRIVATE|UNDOCUMENTED_PIN_API|DEPRECATED_PIN_API
/* PIN API */

/* THIS FILE IS AUTOMAGICALLY GENERATED - DO NOT CHANGE DIRECTLY*/


typedef enum 
{
    EDG_TYPE_INVALID,     ///< illegal type,
    EDG_TYPE_RETURN,      ///< return edge (not used)
    EDG_TYPE_CALL,        ///< call edge (callsite -> callee)
    EDG_TYPE_LINK,        ///< link edge (callsite->returnsite)
    EDG_TYPE_COMP,        ///< compensation edge (not used)
    EDG_TYPE_FALSE,       ///< generic fall through edge (predicated)
    EDG_TYPE_BRANCH,      ///< branch edge
    EDG_TYPE_SWITCH,      ///< switch edge
    EDG_TYPE_PREFETCH,    ///< prefetch edge
    EDG_TYPE_EXCEPTION,   ///< EH edge (callsite -> landing pad )
    EDG_TYPE_NORMAL,      ///< fall through edge (not predicated)
    EDG_TYPE_INDIRECT,    ///< indirect branch (hash lookup -> stub)
    EDG_TYPE_LAST
}EDG_TYPE;

                                                                  /* DO NOT EDIT */
typedef enum 
{
    EDG_STATE_INVALID,
    EDG_STATE_LAST
}EDG_STATE;

                                                                  /* DO NOT EDIT */
extern EDG BBL_SuccEdgFind(BBL src,  EDG_TYPE type);

                                                                  /* DO NOT EDIT */
extern BOOL EDG_InterProcedural(EDG e);

                                                                  /* DO NOT EDIT */
extern BOOL EDG_IsFallthrough(EDG edg);

                                                                  /* DO NOT EDIT */
extern EDG BBL_PredEdgFind(BBL dst,  EDG_TYPE type);

                                                                  /* DO NOT EDIT */
extern EDG BBL_SuccEdgFind(BBL src,  BBL dst);

                                                                  /* DO NOT EDIT */
extern EDG BBL_SuccEdgFindWithType(BBL src,  BBL dst, EDG_TYPE type);

                                                                  /* DO NOT EDIT */
extern EDG BBL_SuccEdgFindPcRelative(BBL bbl);

                                                                  /* DO NOT EDIT */
extern UINT32 BBL_NumPreds(BBL bbl);

                                                                  /* DO NOT EDIT */
extern UINT32 BBL_NumPredsInterproc(BBL bbl);

                                                                  /* DO NOT EDIT */
extern UINT32 BBL_NumPredsInterprocNonCall(BBL bbl);

                                                                  /* DO NOT EDIT */
extern UINT32 BBL_NumSuccs(BBL bbl);

                                                                  /* DO NOT EDIT */

