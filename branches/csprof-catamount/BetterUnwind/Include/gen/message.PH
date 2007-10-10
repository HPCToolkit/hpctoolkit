//Groups: @ingroup\s+(API_REF|IMG_BASIC_API|INS_BASIC_API|INS_INST_API|INS_BASIC_API_GEN_IA32|INS_BASIC_API_IA32|INS_BASIC_API_IPF|INS_MOD_API_GEN_IA32|SEC_BASIC_API|RTN_BASIC_API|REG_BASIC_API|REG_CPU_GENERIC|REG_CPU_IPF|REG_CPU_IA32|TRACE_BASIC_API|BBL_BASIC_API|SYM_BASIC_API|MISC_PRINT|MISC_PARSE|KNOB_API|KNOB_BASIC|KNOB_PRINT|LOCK|PIN_CONTROL|PROTO_API|PIN_THREAD_API|DEBUG_API|TYPE_BASE|INSTLIB|ALARM|ROGUE|ROGUE_CFG|ROGUE_CFG_BASIC_API|ROGUE_EDG_BASIC_API|ROGUE_BBL_BASIC_API|ROGUE_INS_BASIC_API|ROGUE_TRACE_BASIC_API|ROGUE_CONTROL_BASIC_API|CODECACHE_API|SUPERPIN_API|OPTIMIZATION_API|MISC|CHECKPOINTING|PROTO|INST_ARGS|PIN_THREAD_PRIVATE|EDG_BASIC|BBL_BASIC|ROGUE_BASIC_API|CODECACHE_NYI_API|INS_BASIC_API_GEN_IPF|MESSAGE_TYPE|MESSAGE_BASIC|MISC_BASIC|ITC_INST_API|CHECKPOINTING_PRIVATE|UNDOCUMENTED_PIN_API|DEPRECATED_PIN_API
/* PIN API */

/* THIS FILE IS AUTOMAGICALLY GENERATED - DO NOT CHANGE DIRECTLY*/


class MESSAGE_TYPE
{
  private:
    STATIC VOID (*_panic_callback) ();
    STATIC string *_log_file;
    STATIC string *_image_name;
    STATIC MESSAGE_TYPE *_list;

  private:
    const string _name;
    const string _prefix;
    const string _comment;

    BOOL _terminate;
    BOOL _trace;
    BOOL _enabled;
    BOOL _resource;
    BOOL _log;
    // RM: see remarks in constructor
    BOOL _initted;

    MESSAGE_TYPE   *_next;
    
  public:
    STATIC VOID RegisterPanicCallBack( VOID (*cb) () );
    STATIC VOID RegisterLogFile(const string& name );
    STATIC VOID RegisterImageName(const string& name );
    STATIC MESSAGE_TYPE * Find(const string& name);
    STATIC BOOL Disable(const string& name);
    STATIC BOOL Enable(const string& name);
    STATIC UINT32 Number();
    STATIC string StringLongAll(); 
    STATIC BOOL IsTerminating();

  private:
    STATIC VOID DumpTrace();
    
  public:
    MESSAGE_TYPE( string name, string prefix, BOOL terminate, BOOL trace, BOOL enabled, BOOL resource, BOOL log, string comment);
    
    VOID Message(const string& message);
    BOOL on() const {return _enabled;}
    VOID disable() {_enabled=false;}
    VOID enable() {_enabled=true;}

    int Compare(const MESSAGE_TYPE & k2) const;

};

                                                                  /* DO NOT EDIT */
extern MESSAGE_TYPE MessageTypeError;

                                                                  /* DO NOT EDIT */
extern MESSAGE_TYPE MessageTypeWarning;

                                                                  /* DO NOT EDIT */
extern MESSAGE_TYPE MessageTypeConsole;

                                                                  /* DO NOT EDIT */
extern MESSAGE_TYPE MessageTypeAssert;

                                                                  /* DO NOT EDIT */
extern MESSAGE_TYPE MessageTypePhase;

                                                                  /* DO NOT EDIT */
extern MESSAGE_TYPE MessageTypeKnown;

                                                                  /* DO NOT EDIT */
extern MESSAGE_TYPE MessageTypeInfo;

                                                                  /* DO NOT EDIT */
extern MESSAGE_TYPE MessageTypeDebug;

                                                                  /* DO NOT EDIT */
extern MESSAGE_TYPE MessageTypeOpportunity;

                                                                  /* DO NOT EDIT */
extern MESSAGE_TYPE MessageTypeStats;

                                                                  /* DO NOT EDIT */
extern MESSAGE_TYPE MessageTypeLog;

                                                                  /* DO NOT EDIT */
extern const string Line1;

                                                                  /* DO NOT EDIT */
extern UINT64 MilliSecondsElapsed();

                                                                  /* DO NOT EDIT */
extern VOID BreakMe();

                                                                  /* DO NOT EDIT */
/* DO NOT EDIT */

#define QMESSAGE(t,m) do{ if(t.on()) t.Message(m);}while(0)
#define QMESSAGE_LIMIT(t,m,max) do{ static int qqq = 0; if( qqq++ >= max) break; if(t.on()) t.Message(m);}while(0)

#define ASSERTQ(message) QMESSAGE(MessageTypeAssert,message)

#define ASSERT(condition,message)   \
    do{ if(!(condition)) ASSERTQ( string(__FILE__) + ":" + __FUNCTION__  + ":" \
                  + decstr(static_cast<UINT32>(__LINE__)) + ": " + message);} while(0)

#define ASSERTX(condition) ASSERT(condition,"assertion failed: " #condition  "\n")

#define ASSERTSLOW(condition,message)   \
  do{ if(KnobSlowAsserts && !(condition)) \
          ASSERTQ( string(__FILE__) + ":" + __FUNCTION__  + ":" +  \
                  decstr(static_cast<UINT32>(__LINE__)) + ": " + message);} while(0)

#define ASSERTXSLOW(condition) ASSERTSLOW(condition,"assertion failed: " #condition  "\n")

#define NYI()  ASSERT(0, "Not Yet Implemented\n")

#define PIN_ERROR(message) QMESSAGE(MessageTypeError, message)

#define CONSOLE(message) QMESSAGE(MessageTypeConsole, message)

#define PHASE(message) QMESSAGE( MessageTypePhase, message)

#define WARNING(message) QMESSAGE( MessageTypeWarning,message)
#define WARNING_LIMIT(message,n) QMESSAGE_LIMIT( MessageTypeWarning, message, n)

#define KNOWN(message) QMESSAGE( MessageTypeKnown, message)
#define KNOWN_LIMIT(message,n) QMESSAGE_LIMIT( MessageTypeKnown, message, n)

#define LOG(message) QMESSAGE(MessageTypeLog, message)

#define INFO(message) QMESSAGE(MessageTypeInfo,message)
#define INFO_LIMIT(message,n) QMESSAGE_LIMIT(MessageTypeInfo, message, n)

#define OPPORTUNITY(message) QMESSAGE(MessageTypeOpportunity,message)
#define OPPORTUNITY_LIMIT(message,n) QMESSAGE_LIMIT(MessageTypeOpportunity,message, n)
#define DEBUG(message) QMESSAGE(MessageTypeDebug, message)
#define STATS(message) QMESSAGE(MessageTypeStats, message)

