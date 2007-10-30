//Groups: @ingroup\s+(API_REF|IMG_BASIC_API|INS_BASIC_API|INS_INST_API|INS_BASIC_API_GEN_IA32|INS_BASIC_API_IA32|INS_BASIC_API_IPF|INS_MOD_API_GEN_IA32|SEC_BASIC_API|RTN_BASIC_API|REG_BASIC_API|REG_CPU_GENERIC|REG_CPU_IPF|REG_CPU_IA32|TRACE_BASIC_API|BBL_BASIC_API|SYM_BASIC_API|MISC_PRINT|MISC_PARSE|KNOB_API|KNOB_BASIC|KNOB_PRINT|LOCK|PIN_CONTROL|PROTO_API|PIN_THREAD_API|DEBUG_API|TYPE_BASE|INSTLIB|ALARM|ROGUE|ROGUE_CFG|ROGUE_CFG_BASIC_API|ROGUE_EDG_BASIC_API|ROGUE_BBL_BASIC_API|ROGUE_INS_BASIC_API|ROGUE_TRACE_BASIC_API|ROGUE_CONTROL_BASIC_API|CODECACHE_API|SUPERPIN_API|OPTIMIZATION_API|MISC|CHECKPOINTING|PROTO|INST_ARGS|PIN_THREAD_PRIVATE|EDG_BASIC|BBL_BASIC|ROGUE_BASIC_API|CODECACHE_NYI_API|INS_BASIC_API_GEN_IPF|MESSAGE_TYPE|MESSAGE_BASIC|MISC_BASIC|ITC_INST_API|CHECKPOINTING_PRIVATE|UNDOCUMENTED_PIN_API|DEPRECATED_PIN_API
/* PIN API */

/* THIS FILE IS AUTOMAGICALLY GENERATED - DO NOT CHANGE DIRECTLY*/


extern IMG IMG_Next(IMG x);

                                                                  /* DO NOT EDIT */
extern IMG IMG_Prev(IMG x);

                                                                  /* DO NOT EDIT */
extern IMG IMG_Invalid();

                                                                  /* DO NOT EDIT */
extern BOOL IMG_Valid(IMG x);

                                                                  /* DO NOT EDIT */
extern SEC IMG_SecHead(IMG x);

                                                                  /* DO NOT EDIT */
extern SEC IMG_SecTail(IMG x);

                                                                  /* DO NOT EDIT */
extern SYM IMG_RegsymHead(IMG x);

                                                                  /* DO NOT EDIT */
extern ADDRINT IMG_Entry(IMG x);

                                                                  /* DO NOT EDIT */
extern const string& IMG_Name(IMG x);

                                                                  /* DO NOT EDIT */
extern ADDRINT IMG_Gp(IMG x);

                                                                  /* DO NOT EDIT */
extern ADDRINT IMG_LoadOffset(IMG x);

                                                                  /* DO NOT EDIT */
extern ADDRINT IMG_LowAddress(IMG x);

                                                                  /* DO NOT EDIT */
extern ADDRINT IMG_HighAddress(IMG x);

                                                                  /* DO NOT EDIT */
extern  ADDRINT IMG_StartAddress(IMG x);

                                                                  /* DO NOT EDIT */
extern USIZE IMG_SizeMapped(IMG x);

                                                                  /* DO NOT EDIT */
extern IMG_TYPE IMG_Type(IMG x);

                                                                  /* DO NOT EDIT */
extern BOOL IMG_IsMainExecutable(IMG x);

                                                                  /* DO NOT EDIT */
extern IMG SEC_Img(SEC x);

                                                                  /* DO NOT EDIT */
extern SEC SEC_Next(SEC x);

                                                                  /* DO NOT EDIT */
extern SEC SEC_Prev(SEC x);

                                                                  /* DO NOT EDIT */
extern SEC SEC_Invalid();

                                                                  /* DO NOT EDIT */
extern BOOL SEC_Valid(SEC x);

                                                                  /* DO NOT EDIT */
extern RTN SEC_RtnHead(SEC x);

                                                                  /* DO NOT EDIT */
extern RTN SEC_RtnTail(SEC x);

                                                                  /* DO NOT EDIT */
extern const string& SEC_Name(SEC x);

                                                                  /* DO NOT EDIT */
extern SEC_TYPE SEC_Type(SEC x);

                                                                  /* DO NOT EDIT */
extern BOOL SEC_Mapped(SEC sec);

                                                                  /* DO NOT EDIT */
extern const VOID *SEC_Data(SEC x);

                                                                  /* DO NOT EDIT */
extern SEC RTN_Sec(RTN x);

                                                                  /* DO NOT EDIT */
extern RTN RTN_Next(RTN x);

                                                                  /* DO NOT EDIT */
extern RTN RTN_Prev(RTN x);

                                                                  /* DO NOT EDIT */
extern RTN RTN_Invalid();

                                                                  /* DO NOT EDIT */
extern BOOL RTN_Valid(RTN x);

                                                                  /* DO NOT EDIT */
extern BBL RTN_BblHead(RTN x);

                                                                  /* DO NOT EDIT */
extern BBL RTN_BblTail(RTN x);

                                                                  /* DO NOT EDIT */
extern const string& RTN_Name(RTN x);

                                                                  /* DO NOT EDIT */
extern SYM RTN_Sym(RTN x);

                                                                  /* DO NOT EDIT */
extern AFUNPTR RTN_Funptr(RTN x);

                                                                  /* DO NOT EDIT */
inline INT32 RTN_No(RTN x) { return x.q();}

                                                                  /* DO NOT EDIT */
extern INS BBL_InsHead(BBL x);

                                                                  /* DO NOT EDIT */
extern INS BBL_InsTail(BBL x);

                                                                  /* DO NOT EDIT */
extern BBL BBL_Next(BBL x);

                                                                  /* DO NOT EDIT */
extern BBL BBL_Prev(BBL x);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_Valid(BBL x);

                                                                  /* DO NOT EDIT */
extern EDG EDG_NextSucc(EDG x);

                                                                  /* DO NOT EDIT */
extern EDG EDG_INVALID();

                                                                  /* DO NOT EDIT */
extern BOOL EDG_Valid(EDG x);

                                                                  /* DO NOT EDIT */
extern EDG_TYPE EDG_Type(EDG x);

                                                                  /* DO NOT EDIT */
extern BBL_TYPE BBL_Type(BBL x);

                                                                  /* DO NOT EDIT */
extern RTN INS_Rtn(INS x);

                                                                  /* DO NOT EDIT */
extern INS INS_Next(INS x);

                                                                  /* DO NOT EDIT */
extern INS INS_Prev(INS x);

                                                                  /* DO NOT EDIT */
extern INS INS_Invalid();

                                                                  /* DO NOT EDIT */
extern BOOL INS_Valid(INS x);

                                                                  /* DO NOT EDIT */
extern ADDRINT INS_Address(INS ins);

                                                                  /* DO NOT EDIT */
extern USIZE INS_Size(INS ins);

                                                                  /* DO NOT EDIT */
extern ADDRINT INS_DirectBranchOrCallTargetAddress(INS ins);

                                                                  /* DO NOT EDIT */
extern ADDRINT INS_NextAddress(INS ins);

                                                                  /* DO NOT EDIT */
extern SYM SYM_Next(SYM x);

                                                                  /* DO NOT EDIT */
extern SYM SYM_Prev(SYM x);

                                                                  /* DO NOT EDIT */
extern const string& SYM_Name(SYM x);

                                                                  /* DO NOT EDIT */
extern SYM SYM_Invalid();

                                                                  /* DO NOT EDIT */
extern BOOL SYM_Valid(SYM x);

                                                                  /* DO NOT EDIT */
extern BOOL SYM_Dynamic(SYM x);

                                                                  /* DO NOT EDIT */
extern ADDRINT SYM_Value(SYM x);

                                                                  /* DO NOT EDIT */
extern UINT32 SYM_Index(SYM x);

                                                                  /* DO NOT EDIT */
typedef VOID (*IMAGECALLBACK)(IMG, VOID *);

                                                                  /* DO NOT EDIT */
extern UINT32 IMG_Id(IMG x);

                                                                  /* DO NOT EDIT */
extern IMG IMG_FindImgById(UINT32 id);

                                                                  /* DO NOT EDIT */
extern VOID RTN_AddInstrumentFunction(RTN_INSTRUMENT_CALLBACK fun, VOID *val);

                                                                  /* DO NOT EDIT */
extern VOID IMG_AddInstrumentFunction(IMAGECALLBACK fun, VOID *v);

                                                                  /* DO NOT EDIT */
extern VOID IMG_AddUnloadFunction(IMAGECALLBACK fun, VOID *v);

                                                                  /* DO NOT EDIT */
extern USIZE RTN_Size(RTN rtn);

                                                                  /* DO NOT EDIT */
extern IMG IMG_Open(const string& filename);

                                                                  /* DO NOT EDIT */
extern VOID IMG_Close(IMG img);

                                                                  /* DO NOT EDIT */
extern string RTN_FindNameByAddress(ADDRINT address);

                                                                  /* DO NOT EDIT */
extern RTN RTN_FindByAddress(ADDRINT address);

                                                                  /* DO NOT EDIT */
extern RTN RTN_FindByName(IMG img, const CHAR * name);

                                                                  /* DO NOT EDIT */
extern VOID RTN_Open(RTN rtn);

                                                                  /* DO NOT EDIT */
extern VOID RTN_Close(RTN rtn);

                                                                  /* DO NOT EDIT */
extern INS RTN_InsHead(RTN rtn);

                                                                  /* DO NOT EDIT */
extern VOID RTN_InsertCall(RTN rtn, IPOINT action, AFUNPTR funptr, ...);

                                                                  /* DO NOT EDIT */
extern ADDRINT RTN_Address(RTN rtn);

                                                                  /* DO NOT EDIT */
extern ADDRINT SEC_Address(SEC sec);

                                                                  /* DO NOT EDIT */
extern BOOL SEC_IsReadable(SEC sec);

                                                                  /* DO NOT EDIT */
extern BOOL SEC_IsWriteable(SEC sec);

                                                                  /* DO NOT EDIT */
extern BOOL SEC_IsExecutable(SEC sec);

                                                                  /* DO NOT EDIT */
extern USIZE SEC_Size(SEC sec);

                                                                  /* DO NOT EDIT */
extern BOOL BBL_Original(BBL bbl);

                                                                  /* DO NOT EDIT */
extern ADDRINT BBL_Address(BBL bbl);

                                                                  /* DO NOT EDIT */
extern ADDRINT BBL_NextAddress(BBL bbl);

                                                                  /* DO NOT EDIT */
extern USIZE BBL_Size(BBL bbl);

                                                                  /* DO NOT EDIT */
extern IMG APP_ImgHead();

                                                                  /* DO NOT EDIT */
extern IMG APP_ImgTail();

                                                                  /* DO NOT EDIT */

