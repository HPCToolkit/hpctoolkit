//Groups: @ingroup\s+(API_REF|IMG_BASIC_API|INS_BASIC_API|INS_INST_API|INS_BASIC_API_GEN_IA32|INS_BASIC_API_IA32|INS_BASIC_API_IPF|INS_MOD_API_GEN_IA32|SEC_BASIC_API|RTN_BASIC_API|REG_BASIC_API|REG_CPU_GENERIC|REG_CPU_IPF|REG_CPU_IA32|TRACE_BASIC_API|BBL_BASIC_API|SYM_BASIC_API|MISC_PRINT|MISC_PARSE|KNOB_API|KNOB_BASIC|KNOB_PRINT|LOCK|PIN_CONTROL|PROTO_API|PIN_THREAD_API|DEBUG_API|TYPE_BASE|INSTLIB|ALARM|ROGUE|ROGUE_CFG|ROGUE_CFG_BASIC_API|ROGUE_EDG_BASIC_API|ROGUE_BBL_BASIC_API|ROGUE_INS_BASIC_API|ROGUE_TRACE_BASIC_API|ROGUE_CONTROL_BASIC_API|CODECACHE_API|SUPERPIN_API|OPTIMIZATION_API|MISC|CHECKPOINTING|PROTO|INST_ARGS|PIN_THREAD_PRIVATE|EDG_BASIC|BBL_BASIC|ROGUE_BASIC_API|CODECACHE_NYI_API|INS_BASIC_API_GEN_IPF|MESSAGE_TYPE|MESSAGE_BASIC|MISC_BASIC|ITC_INST_API|CHECKPOINTING_PRIVATE|UNDOCUMENTED_PIN_API|DEPRECATED_PIN_API
/* PIN API */

/* THIS FILE IS AUTOMAGICALLY GENERATED - DO NOT CHANGE DIRECTLY*/


extern INT32 INS_Category(const INS ins);

                                                                  /* DO NOT EDIT */
extern INT32 INS_Extension(const INS ins);

                                                                  /* DO NOT EDIT */
extern USIZE INS_MemoryWriteSize(INS ins);

                                                                  /* DO NOT EDIT */
extern USIZE INS_MemoryWriteSizeWithoutPrefix(INS ins);

                                                                  /* DO NOT EDIT */
extern USIZE INS_MemoryReadSize(INS ins);

                                                                  /* DO NOT EDIT */
extern USIZE INS_MemoryReadSizeWithoutPrefix(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsMemoryRead(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsMemoryWrite(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_HasMemoryRead2(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_HasFallThrough(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsSyscall(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsSysenter(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsNop(INS ins);

                                                                  /* DO NOT EDIT */
extern string OPCODE_StringShort(UINT32 opcode);

                                                                  /* DO NOT EDIT */
extern string INS_Mnemonic(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsBranch(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsHalt(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsBranchOrCall(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsPcMaterialization(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsCall(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsFarCall(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsFarJump(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsDirectFarJump(INS ins);

                                                                  /* DO NOT EDIT */
extern VOID INS_GetFarPointer(INS ins, 
                                 UINT16& segment_selector,
                                 UINT32& displacement);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsProcedureCall(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsInterrupt(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsRet(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsPrefetch(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsSub(const INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsMov(const INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsAtomicUpdate(const INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsRDTSC(const INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsIndirectBranchOrCall(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsDirectBranchOrCall(INS ins);

                                                                  /* DO NOT EDIT */
extern REG INS_SegmentRegPrefix(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsRewritableMemOpBaseLimited(INS ins, MEMORY_TYPE mtype, REG & base);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsRewritableMemOpBase(INS ins, MEMORY_TYPE mtype, REG & base);

                                                                  /* DO NOT EDIT */
extern VOID PIN_SetSyntaxIntel();

                                                                  /* DO NOT EDIT */
extern VOID PIN_SetSyntaxATT();

                                                                  /* DO NOT EDIT */
extern VOID PIN_SetSyntaxXED();

                                                                  /* DO NOT EDIT */
extern REG INS_RegR(INS x, UINT32 k);

                                                                  /* DO NOT EDIT */
extern REG INS_RegW(INS x, UINT32 k);

                                                                  /* DO NOT EDIT */
extern OPCODE INS_Opcode(INS ins);

                                                                  /* DO NOT EDIT */
extern string CATEGORY_StringShort(UINT32 num);

                                                                  /* DO NOT EDIT */
extern string EXTENSION_StringShort(UINT32 num);

                                                                  /* DO NOT EDIT */
extern BOOL INS_SegPrefixIsMemoryRead(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_SegPrefixIsMemoryWrite(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_AddressSizePrefix(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_BranchNotTakenPrefix(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_BranchTakenPrefix(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_LockPrefix(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_OperandSizePrefix(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_Refining66Prefix(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_RefiningF2Prefix(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_RefiningF3Prefix(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_RepPrefix(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_RepnePrefix(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_SegmentPrefix(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsXchg(INS ins);

                                                                  /* DO NOT EDIT */
extern BOOL INS_IsIRet(INS ins);

                                                                  /* DO NOT EDIT */

