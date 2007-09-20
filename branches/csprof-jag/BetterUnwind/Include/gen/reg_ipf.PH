//Groups: @ingroup\s+(API_REF|IMG_BASIC_API|INS_BASIC_API|INS_INST_API|INS_BASIC_API_GEN_IA32|INS_BASIC_API_IA32|INS_BASIC_API_IPF|INS_MOD_API_GEN_IA32|SEC_BASIC_API|RTN_BASIC_API|REG_BASIC_API|REG_CPU_GENERIC|REG_CPU_IPF|REG_CPU_IA32|TRACE_BASIC_API|BBL_BASIC_API|SYM_BASIC_API|MISC_PRINT|MISC_PARSE|KNOB_API|KNOB_BASIC|KNOB_PRINT|LOCK|PIN_CONTROL|PROTO_API|PIN_THREAD_API|DEBUG_API|TYPE_BASE|INSTLIB|ALARM|ROGUE|ROGUE_CFG|ROGUE_CFG_BASIC_API|ROGUE_EDG_BASIC_API|ROGUE_BBL_BASIC_API|ROGUE_INS_BASIC_API|ROGUE_TRACE_BASIC_API|ROGUE_CONTROL_BASIC_API|CODECACHE_API|SUPERPIN_API|OPTIMIZATION_API|MISC|CHECKPOINTING|PROTO|INST_ARGS|PIN_THREAD_PRIVATE|EDG_BASIC|BBL_BASIC|ROGUE_BASIC_API|CODECACHE_NYI_API|INS_BASIC_API_GEN_IPF|MESSAGE_TYPE|MESSAGE_BASIC|MISC_BASIC|ITC_INST_API|CHECKPOINTING_PRIVATE|UNDOCUMENTED_PIN_API|DEPRECATED_PIN_API
/* PIN API */

/* THIS FILE IS AUTOMAGICALLY GENERATED - DO NOT CHANGE DIRECTLY*/


typedef enum
{
    REG_INVALIDx = 0,   // REG_INVALID is a function returning this value.
    REG_NONE = 1,
    REG_IMMBASE = 2,
    REG_IMM_LIT = REG_IMMBASE + 0,
    REG_IMM_BBL = REG_IMMBASE + 1,
    REG_IMM_REL = REG_IMMBASE + 2,

    
    REG_RBASE,                              // register base
    REG_MACHINE_BASE = REG_RBASE,           // real registers on the machine
                                            //   actually, this is registers
                                            //   to be allocated -- needs to be
                                            //   rethought for IPF
    
    REG_APPLICATION_BASE = REG_RBASE,       // register names used by the application
    REG_PHYSICAL_CONTEXT_BEGIN = REG_RBASE, // registers saved in PCTXT
    
    REG_BBASE = REG_RBASE, // 8 branch registers
    REG_B0 = REG_BBASE + 0,
    REG_B1 = REG_BBASE + 1,
    REG_B2 = REG_BBASE + 2,
    REG_B3 = REG_BBASE + 3,
    REG_B4 = REG_BBASE + 4,
    REG_B5 = REG_BBASE + 5,
    REG_B6 = REG_BBASE + 6,
    REG_B7 = REG_BBASE + 7,

    REG_GBASE, // 32 non-rotating general registers
    REG_GR_BASE = REG_GBASE,
    REG_GZERO = REG_GBASE + 0,
    REG_GP  = REG_GBASE + 1,
    REG_G01 = REG_GP,
    REG_G02 = REG_GBASE + 2,
    REG_G03 = REG_GBASE + 3,
    REG_G04 = REG_GBASE + 4,
    REG_G05 = REG_GBASE + 5,
    REG_G06 = REG_GBASE + 6,
    REG_G07 = REG_GBASE + 7,
    REG_G08 = REG_GBASE + 8,
    REG_G09 = REG_GBASE + 9,
    REG_G10 = REG_GBASE + 10,
    REG_G11 = REG_GBASE + 11,
    REG_STACK_PTR  = REG_GBASE + 12, // generic, isa-independent stack pointer
    REG_G12 = REG_STACK_PTR,
    REG_TP  = REG_GBASE + 13,
    REG_G13 = REG_TP,
    REG_G14 = REG_GBASE + 14,
    REG_G15 = REG_GBASE + 15,
    REG_G16 = REG_GBASE + 16,
    REG_G17 = REG_GBASE + 17,
    REG_G18 = REG_GBASE + 18,
    REG_G19 = REG_GBASE + 19,
    REG_G20 = REG_GBASE + 20,
    REG_G21 = REG_GBASE + 21,
    REG_G22 = REG_GBASE + 22,
    REG_G23 = REG_GBASE + 23,
    REG_G24 = REG_GBASE + 24,
    REG_G25 = REG_GBASE + 25,
    REG_G26 = REG_GBASE + 26,
    REG_G27 = REG_GBASE + 27,
    REG_G28 = REG_GBASE + 28,
    REG_G29 = REG_GBASE + 29,
    REG_G30 = REG_GBASE + 30,
    REG_G31 = REG_GBASE + 31,
    REG_GROT_BASE,
    REG_G32 = REG_GROT_BASE -32 + 32,  // these need to be contiguous with g0-g31
    REG_G33 = REG_GROT_BASE -32 + 33,
    REG_G34 = REG_GROT_BASE -32 + 34,
    REG_G35 = REG_GROT_BASE -32 + 35,
    REG_G36 = REG_GROT_BASE -32 + 36,
    REG_G37 = REG_GROT_BASE -32 + 37,
    REG_G38 = REG_GROT_BASE -32 + 38,
    REG_G39 = REG_GROT_BASE -32 + 39,
    REG_G40 = REG_GROT_BASE -32 + 40,
    REG_G41 = REG_GROT_BASE -32 + 41,
    REG_G42 = REG_GROT_BASE -32 + 42,
    REG_G43 = REG_GROT_BASE -32 + 43,
    REG_G105 = REG_GROT_BASE -32 + 105,
    REG_G106 = REG_GROT_BASE -32 + 106,
    REG_G127 = REG_GROT_BASE -32 + 127,
    REG_GROT_LAST = REG_G127,
    REG_GR_LAST = REG_GROT_LAST,


    REG_FBASE,   // 128 floating point registers
    REG_FZERO = REG_FBASE + 0,
    REG_FONE  = REG_FBASE + 1,
    REG_F01   = REG_FBASE + 1, 
    REG_F02   = REG_FBASE + 2,
    REG_F03   = REG_FBASE + 3,
    REG_F04   = REG_FBASE + 4,
    REG_F05   = REG_FBASE + 5,
    REG_F06   = REG_FBASE + 6,
    REG_F07   = REG_FBASE + 7,
    REG_F08   = REG_FBASE + 8,
    REG_F09   = REG_FBASE + 9,
    REG_F10   = REG_FBASE + 10,
    REG_F11   = REG_FBASE + 11,
    REG_F12   = REG_FBASE + 12,
    REG_F13   = REG_FBASE + 13,
    REG_F14   = REG_FBASE + 14,
    REG_F15   = REG_FBASE + 15,
    REG_F16   = REG_FBASE + 16,
    REG_F17   = REG_FBASE + 17,
    REG_F18   = REG_FBASE + 18,
    REG_F19   = REG_FBASE + 19,
    REG_F20   = REG_FBASE + 20,
    REG_F21   = REG_FBASE + 21,
    REG_F22   = REG_FBASE + 22,
    REG_F23   = REG_FBASE + 23,
    REG_F24   = REG_FBASE + 24,
    REG_F25   = REG_FBASE + 25,
    REG_F26   = REG_FBASE + 26,
    REG_F27   = REG_FBASE + 27,
    REG_F28   = REG_FBASE + 28,
    REG_F29   = REG_FBASE + 29,
    REG_F30   = REG_FBASE + 30,
    REG_F31   = REG_FBASE + 31,
    REG_FROT_BASE,
    REG_F32   = REG_FROT_BASE -32 + 32,  // these need to be contiguous with f0-f31
    REG_F127  = REG_FROT_BASE - 32 + 127,


    REG_ABASE,   // 128 application registers (implicit) 

    REG_AR_KR0 = REG_ABASE + 0,
    REG_AR_KR1 = REG_ABASE + 1,
    REG_AR_KR2 = REG_ABASE + 2,
    REG_AR_KR3 = REG_ABASE + 3,
    REG_AR_KR4 = REG_ABASE + 4,
    REG_AR_KR5 = REG_ABASE + 5,
    REG_AR_KR6 = REG_ABASE + 6,
    REG_AR_KR7 = REG_ABASE + 7,

    
    REG_AR_RSC   = REG_ABASE + 16,
    REG_AR_BSP     = REG_ABASE + 17,
    REG_AR_RNAT    = REG_ABASE + 19,
    
    REG_AR_CSD = REG_ABASE + 25,
    REG_AR_SSD = REG_ABASE + 26,
    
    REG_AR_CCV     = REG_ABASE + 32,
    REG_AR_UNAT  =  REG_ABASE + 36,

    REG_AR_FPSR  =  REG_ABASE + 40,

    REG_AR_PFS   = REG_ABASE + 64,
    REG_AR_LC    = REG_ABASE + 65,
    REG_AR_EC    = REG_ABASE + 66,

    REG_AR_LAST  = REG_ABASE + 127,

    REG_SBASE, // special registers

    
    REG_PR    = REG_SBASE,
    REG_CFM,
    REG_INST_PTR,   // generic, isa-independent inst pointer
    REG_NAT_LOW,    // nat bits for gr0-gr63
    REG_NAT_HIGH,   // nat bits for gr64-gr127
    REG_UM,         // user mask
    
    REG_SR_LAST = REG_UM,
    
    REG_PHYSICAL_CONTEXT_END = REG_SR_LAST,
    REG_MACHINE_LAST = REG_PHYSICAL_CONTEXT_END,

    REG_PBASE, // 64 predicate registers 
    REG_PR0 = REG_PBASE + 0,
    REG_PR1 = REG_PBASE + 1,
    REG_PR2 = REG_PBASE + 2,
    REG_PR3 = REG_PBASE + 3,
    REG_PR4 = REG_PBASE + 4,
    REG_PR5 = REG_PBASE + 5,
    REG_PR6 = REG_PBASE + 6,
    REG_PR7 = REG_PBASE + 7,
    REG_PR8 = REG_PBASE + 8,
    REG_PR9 = REG_PBASE + 9,
    REG_PR15 = REG_PBASE + 15,
    REG_PR63 = REG_PBASE + 63,

    REG_APPLICATION_LAST = REG_PR63,

    // Pin's virtual register names
    REG_PIN_BASE,
    REG_PIN_GR_BASE = REG_PIN_BASE,
    REG_PIN_STACK_PTR, 

    /* ! @todoshould be REG_PIN_THREAD_ID ?*/
    // Every thread is assigned an index so we can implement tls
    REG_THREAD_ID,

    // ISA-independent gr regs
    REG_PIN_INDIRREG,  // virtual reg holding indirect jmp target value
    REG_PIN_IPRELADDR, // virtual reg holding ip-rel address value
    REG_PIN_TEMPOFFSET,  // temporary storage offset
    
    // ISA-independent gr regs holding temporary values
    REG_PIN_T_BASE,
    REG_PIN_T0 = REG_PIN_T_BASE,        
    REG_PIN_T1,        
    REG_PIN_T2,
    REG_PIN_T3,
    REG_PIN_T_LAST = REG_PIN_T3,

    // ISA-independent Pin virtual regs needed for instrumentation
    REG_INST_BASE,
    REG_INST_SCRATCH_BASE = REG_INST_BASE,
    REG_INST_G0 = REG_INST_SCRATCH_BASE,
    REG_INST_G1,
    REG_INST_G2,
    REG_INST_G3,
    REG_INST_G4,
    REG_INST_G5,
    REG_INST_G6,
    REG_INST_G7,
    REG_INST_G8,
    REG_INST_G9,
    REG_INST_SCRATCH_LAST = REG_INST_G9,
    REG_INST_LAST = REG_INST_G9,
    
    // Virtual regs used by Pin inside instrumentation bridges.
    // Unlike REG_INST_BASE to REG_INST_LAST, these registers are
    // NOT visible to  Pin clients.
    //
    REG_INST_ARG_BASE,
    REG_INST_ARG_G0 = REG_INST_ARG_BASE,
    REG_INST_ARG_G1,
    REG_INST_ARG_G2,
    REG_INST_ARG_G3,
    REG_INST_ARG_G4,
    REG_INST_ARG_G5,
    REG_INST_ARG_G6,
    REG_INST_ARG_G7,
    REG_INST_ARG_G8,
    REG_INST_ARG_G9,
    REG_INST_ARG_LAST = REG_INST_ARG_G9,
    
    // Instrumentation manually saves the application values here
    REG_SPILL_PFS,
    REG_SPILL_B1,
    REG_SPILL_COVERING_PFS,
    REG_SPILL_COVERING_B1,
    REG_SPILL_PR1,

    REG_INST_COND, // virtual register used in conditional instrumentation
    
    REG_PIN_BRIDGE_TOOL_TP, // Tool thread pointer in bridge
    REG_PIN_FLOATING_RRB, // a copy of the floating rotating register base is maintained here.
    REG_PIN_SPILLPTR, // ptr to the pin spill area
    REG_PIN_SPILL_SCRATCH, // scratch register used when spilling (only for rotating floating registers)
    REG_PIN_DYNAMIC_PROFILE_SCRATCH, // This is used only when dynamic profiling is turned on
    REG_PIN_GR_LAST=REG_PIN_SPILLPTR,
    
    REG_PIN_SPILLF02,
    
    // IPF-specific Pin GRs used for inlining of analysis functions
    REG_PIN_G01,
    REG_PIN_GP =  REG_PIN_G01,
    REG_PIN_G02 = REG_PIN_G01 + 1,
    REG_PIN_G03 = REG_PIN_G01 + 2,
    REG_PIN_G04 = REG_PIN_G01 + 3,
    REG_PIN_G05 = REG_PIN_G01 + 4,
    REG_PIN_G06 = REG_PIN_G01 + 5,
    REG_PIN_G07 = REG_PIN_G01 + 6,
    REG_PIN_G08 = REG_PIN_G01 + 7,
    REG_PIN_G09 = REG_PIN_G01 + 8,
    REG_PIN_G10 = REG_PIN_G01 + 9,
    REG_PIN_G11 = REG_PIN_G01 + 10,
    REG_PIN_G12 = REG_PIN_G01 + 11,
    REG_PIN_G13 = REG_PIN_G01 + 12,
    REG_PIN_G14 = REG_PIN_G01 + 13,
    REG_PIN_G15 = REG_PIN_G01 + 14,
    REG_PIN_G16 = REG_PIN_G01 + 15,
    REG_PIN_G17 = REG_PIN_G01 + 16,
    REG_PIN_G18 = REG_PIN_G01 + 17,
    REG_PIN_G19 = REG_PIN_G01 + 18,
    REG_PIN_G20 = REG_PIN_G01 + 19,
    REG_PIN_G21 = REG_PIN_G01 + 20,
    REG_PIN_G22 = REG_PIN_G01 + 21,
    REG_PIN_G23 = REG_PIN_G01 + 22,
    REG_PIN_G24 = REG_PIN_G01 + 23,
    REG_PIN_G25 = REG_PIN_G01 + 24,
    REG_PIN_G26 = REG_PIN_G01 + 25,
    REG_PIN_G27 = REG_PIN_G01 + 26,
    REG_PIN_G28 = REG_PIN_G01 + 27,
    REG_PIN_G29 = REG_PIN_G01 + 28,
    REG_PIN_G30 = REG_PIN_G01 + 29,
    REG_PIN_G31 = REG_PIN_G01 + 30,
    REG_PIN_G32 = REG_PIN_G01 + 31,
    REG_PIN_G33 = REG_PIN_G01 + 32,
    REG_PIN_G34 = REG_PIN_G01 + 33,
    REG_PIN_G35 = REG_PIN_G01 + 34,
    REG_PIN_G36 = REG_PIN_G01 + 35,
    REG_PIN_G37 = REG_PIN_G01 + 36,
    REG_PIN_G38 = REG_PIN_G01 + 37,
    REG_PIN_G39 = REG_PIN_G01 + 38,
    REG_PIN_G40 = REG_PIN_G01 + 39,
    REG_PIN_G41 = REG_PIN_G01 + 40,
    REG_PIN_G42 = REG_PIN_G01 + 41,
    REG_PIN_G43 = REG_PIN_G01 + 42,

    REG_PIN_INLINE_LAST = REG_PIN_G43,
    // Floating point registers shadowing FP argument regs used
    // for inlining of analysis functions
    REG_PIN_FARG_BASE,
    REG_PIN_FARG0 = REG_PIN_FARG_BASE + 0,
    REG_PIN_FARG1 = REG_PIN_FARG_BASE + 1,
    REG_PIN_FARG2 = REG_PIN_FARG_BASE + 2,
    REG_PIN_FARG3 = REG_PIN_FARG_BASE + 3,
    REG_PIN_FARG4 = REG_PIN_FARG_BASE + 4,
    REG_PIN_FARG5 = REG_PIN_FARG_BASE + 5,
    REG_PIN_FARG6 = REG_PIN_FARG_BASE + 6,
    REG_PIN_FARG7 = REG_PIN_FARG_BASE + 7,
    
    REG_PIN_LAST = REG_PIN_FARG7,
    
    REG_LAST
    
} REG;

                                                                  /* DO NOT EDIT */
const ADDRINT NUM_PHYSICAL_REGS = REG_PHYSICAL_CONTEXT_END - REG_PHYSICAL_CONTEXT_BEGIN + 1;

                                                                  /* DO NOT EDIT */
const ADDRINT NUM_SPILLED_REGS =  NUM_PHYSICAL_REGS + (REG_PIN_LAST - REG_PIN_BASE + 1);

                                                                  /* DO NOT EDIT */
const ADDRINT NUM_FP_REGS = REG_F127 - REG_FBASE + 1;

                                                                  /* DO NOT EDIT */
const ADDRINT NUM_SCRATCH_REGS = REG_INST_SCRATCH_LAST - REG_INST_SCRATCH_BASE + 1;

                                                                  /* DO NOT EDIT */
const ADDRINT NUM_SPECIAL_REGS = NUM_SCRATCH_REGS;

                                                                  /* DO NOT EDIT */
inline BOOL REG_is_gr(REG reg)
{
    return ((reg >= REG_GBASE) && (reg < REG_GBASE+32)) ||
                    ((reg >= REG_G32) && (reg <= REG_G127));
}

                                                                  /* DO NOT EDIT */
inline BOOL REG_is_fr(REG reg)
{
    return ((reg >= REG_FBASE) && (reg < REG_FBASE+32)) ||
                    ((reg >= REG_FBASE + 32) && (reg < REG_FBASE+128));
}

                                                                  /* DO NOT EDIT */
inline BOOL REG_is_seg(REG reg)
{
    return FALSE;
}

                                                                  /* DO NOT EDIT */
inline BOOL REG_is_br(REG reg)
{
    return (reg >= REG_BBASE) && (reg < REG_BBASE+8);
}

                                                                  /* DO NOT EDIT */
inline BOOL REG_is_pr(REG reg){ return (reg >= REG_PBASE) && (reg < REG_PBASE+64);}

                                                                  /* DO NOT EDIT */
extern string REG_StringShort(REG reg);

                                                                  /* DO NOT EDIT */
extern REG REG_FullRegName(const REG reg);

                                                                  /* DO NOT EDIT */

