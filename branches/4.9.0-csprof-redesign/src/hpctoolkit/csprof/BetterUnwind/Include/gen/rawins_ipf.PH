//Groups: @ingroup\s+(API_REF|IMG_BASIC_API|INS_BASIC_API|INS_INST_API|INS_BASIC_API_GEN_IA32|INS_BASIC_API_IA32|INS_BASIC_API_IPF|INS_MOD_API_GEN_IA32|SEC_BASIC_API|RTN_BASIC_API|REG_BASIC_API|REG_CPU_GENERIC|REG_CPU_IPF|REG_CPU_IA32|TRACE_BASIC_API|BBL_BASIC_API|SYM_BASIC_API|MISC_PRINT|MISC_PARSE|KNOB_API|KNOB_BASIC|KNOB_PRINT|LOCK|PIN_CONTROL|PROTO_API|PIN_THREAD_API|DEBUG_API|TYPE_BASE|INSTLIB|ALARM|ROGUE|ROGUE_CFG|ROGUE_CFG_BASIC_API|ROGUE_EDG_BASIC_API|ROGUE_BBL_BASIC_API|ROGUE_INS_BASIC_API|ROGUE_TRACE_BASIC_API|ROGUE_CONTROL_BASIC_API|CODECACHE_API|SUPERPIN_API|OPTIMIZATION_API|MISC|CHECKPOINTING|PROTO|INST_ARGS|PIN_THREAD_PRIVATE|EDG_BASIC|BBL_BASIC|ROGUE_BASIC_API|CODECACHE_NYI_API|INS_BASIC_API_GEN_IPF|MESSAGE_TYPE|MESSAGE_BASIC|MISC_BASIC|ITC_INST_API|CHECKPOINTING_PRIVATE|UNDOCUMENTED_PIN_API|DEPRECATED_PIN_API
/* PIN API */

/* THIS FILE IS AUTOMAGICALLY GENERATED - DO NOT CHANGE DIRECTLY*/


typedef enum
{
    TYPE_CMP_INVALID,
    TYPE_CMP_LT,
    TYPE_CMP_LTU,
    TYPE_CMP_LE,
    
    TYPE_CMP_EQ,
    TYPE_CMP_NE,
    
    TYPE_CMP_GT,
    TYPE_CMP_GE,
    TYPE_CMP_Z,
    TYPE_CMP_NZ,

    TYPE_CMP_UNORD,
    TYPE_CMP_LAST
}TYPE_CMP;

                                                                  /* DO NOT EDIT */
typedef enum
{
    TYPE_BWH_INVALID,
    TYPE_BWH_SPNT,
    TYPE_BWH_SPTK,
    TYPE_BWH_DPNT,
    TYPE_BWH_DPTK,
    TYPE_BWH_LAST
}TYPE_BWH;

                                                                  /* DO NOT EDIT */
typedef enum
{
    TYPE_PH_INVALID,
    TYPE_PH_FEW,
    TYPE_PH_MANY,
    TYPE_PH_LAST
}TYPE_PH;

                                                                  /* DO NOT EDIT */
typedef enum
{
    TYPE_BRA_INVALID,
    TYPE_BRA_COND, // jmp and branch
    TYPE_BRA_WEXIT,
    TYPE_BRA_WTOP,
    TYPE_BRA_CEXIT,
    TYPE_BRA_CTOP,
    TYPE_BRA_CLOOP,
    TYPE_BRA_CALL, // jmp and branch
    TYPE_BRA_CALL_PSEUDO, // side-effects only
    TYPE_BRA_RET_PSEUDO, // side-effects only

    TYPE_BRA_IA, // jmp only
    TYPE_BRA_RET, // jmp only
    TYPE_BRA_LAST    
}TYPE_BRA;

                                                                  /* DO NOT EDIT */
typedef enum
{
    TYPE_CHK_INVALID,
    TYPE_CHK_NAT,
    TYPE_CHK_ALAT,
    TYPE_CHK_FPFLAGS,
    TYPE_CHK_LAST
}TYPE_CHK;

                                                                  /* DO NOT EDIT */
typedef enum
{
    TYPE_MEM_INVALID,
    TYPE_MEM_NORMAL,
    TYPE_MEM_S,
    TYPE_MEM_A,
    TYPE_MEM_SA,
    TYPE_MEM_BIAS,
    TYPE_MEM_ACQ,
    TYPE_MEM_FILL,
    TYPE_MEM_CCLR,
    TYPE_MEM_CNC,
    TYPE_MEM_CCLRACQ,
    TYPE_MEM_REL,
    TYPE_MEM_SPILL,
    TYPE_MEM_EXCL,
    TYPE_MEM_FAULT,
    TYPE_MEM_FAULTEXCL,
    TYPE_MEM_LAST
}TYPE_MEM;

                                                                  /* DO NOT EDIT */
typedef enum
{
    TYPE_MEMHINT_INVALID,
    TYPE_MEMHINT_NORMAL,
    TYPE_MEMHINT_NT1,
    TYPE_MEMHINT_NT2,
    TYPE_MEMHINT_NTA,
    TYPE_MEMHINT_LAST
}TYPE_MEMHINT;

                                                                  /* DO NOT EDIT */
typedef enum
{
    TYPE_PRED_INVALID,
    TYPE_PRED_NORMAL,
    TYPE_PRED_UNC,
    TYPE_PRED_AND,
    TYPE_PRED_OR,
    TYPE_PRED_ORANDCM,
    TYPE_PRED_LAST
}TYPE_PRED;

                                                                  /* DO NOT EDIT */
typedef enum
{   
    TYPE_SIZE_INVALID,
    // for int loads 
    TYPE_SIZE_1,
    TYPE_SIZE_2,
    TYPE_SIZE_4,
    TYPE_SIZE_8,
    TYPE_SIZE_16,
    
    // for fp loads
    TYPE_SIZE_FS,
    TYPE_SIZE_FD,
    TYPE_SIZE_F8,
    TYPE_SIZE_FE,
    TYPE_SIZE_FF, /* fp fill and spill */
    TYPE_SIZE_FPS,
    TYPE_SIZE_FPD,
    TYPE_SIZE_FP8,
    
    TYPE_SIZE_F, /* other general fp operation */
    TYPE_SIZE_PREFETCH, /* size type of prefetch instruction */
    TYPE_SIZE_LAST
}TYPE_SIZE;

                                                                  /* DO NOT EDIT */

