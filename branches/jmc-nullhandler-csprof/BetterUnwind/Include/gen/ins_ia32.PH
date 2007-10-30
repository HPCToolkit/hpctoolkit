//Groups: @ingroup\s+(API_REF|IMG_BASIC_API|INS_BASIC_API|INS_INST_API|INS_BASIC_API_GEN_IA32|INS_BASIC_API_IA32|INS_BASIC_API_IPF|INS_MOD_API_GEN_IA32|SEC_BASIC_API|RTN_BASIC_API|REG_BASIC_API|REG_CPU_GENERIC|REG_CPU_IPF|REG_CPU_IA32|TRACE_BASIC_API|BBL_BASIC_API|SYM_BASIC_API|MISC_PRINT|MISC_PARSE|KNOB_API|KNOB_BASIC|KNOB_PRINT|LOCK|PIN_CONTROL|PROTO_API|PIN_THREAD_API|DEBUG_API|TYPE_BASE|INSTLIB|ALARM|ROGUE|ROGUE_CFG|ROGUE_CFG_BASIC_API|ROGUE_EDG_BASIC_API|ROGUE_BBL_BASIC_API|ROGUE_INS_BASIC_API|ROGUE_TRACE_BASIC_API|ROGUE_CONTROL_BASIC_API|CODECACHE_API|SUPERPIN_API|OPTIMIZATION_API|MISC|CHECKPOINTING|PROTO|INST_ARGS|PIN_THREAD_PRIVATE|EDG_BASIC|BBL_BASIC|ROGUE_BASIC_API|CODECACHE_NYI_API|INS_BASIC_API_GEN_IPF|MESSAGE_TYPE|MESSAGE_BASIC|MISC_BASIC|ITC_INST_API|CHECKPOINTING_PRIVATE|UNDOCUMENTED_PIN_API|DEPRECATED_PIN_API
/* PIN API */

/* THIS FILE IS AUTOMAGICALLY GENERATED - DO NOT CHANGE DIRECTLY*/


extern USIZE INS_MaxProbeSize();

                                                                  /* DO NOT EDIT */
extern UINT32 INS_MaxNumRRegs(INS x);

                                                                  /* DO NOT EDIT */
extern UINT32 INS_MaxNumWRegs(INS x);

                                                                  /* DO NOT EDIT */
extern BOOL INS_RegRContain(const INS ins, const REG reg);

                                                                  /* DO NOT EDIT */
extern BOOL INS_RegWContain(const INS ins, const REG reg);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsStackRead(const INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsStackWrite(const INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsIpRelRead(const INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsIpRelWrite(const INS ins);

                                                                  /* DO NOT EDIT */
extern PREDICATE INS_GetPredicate(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsPredicated(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsOriginal(INS ins);

                                                                  /* DO NOT EDIT */
extern string INS_Disassemble(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsPatchable(INS ins);

                                                                  /* DO NOT EDIT */
extern ADDRINT INS_MemoryOffset(INS ins);

                                                                  /* DO NOT EDIT */
extern ADDRINT INS_MemoryDisplacement(INS ins);

                                                                  /* DO NOT EDIT */
extern ADDRDELTA INS_MemoryDisplacementSigned(INS ins);

                                                                  /* DO NOT EDIT */
extern REG INS_MemoryBaseReg(INS ins);

                                                                  /* DO NOT EDIT */
extern REG INS_MemoryIndexReg(INS ins);

                                                                  /* DO NOT EDIT */
extern UINT32 INS_MemoryScale(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_HasProperty(INS ins, INS_PROP prop);

                                                                  /* DO NOT EDIT */

