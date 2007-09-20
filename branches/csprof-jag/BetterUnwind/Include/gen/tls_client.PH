//Groups: @ingroup\s+(API_REF|IMG_BASIC_API|INS_BASIC_API|INS_INST_API|INS_BASIC_API_GEN_IA32|INS_BASIC_API_IA32|INS_BASIC_API_IPF|INS_MOD_API_GEN_IA32|SEC_BASIC_API|RTN_BASIC_API|REG_BASIC_API|REG_CPU_GENERIC|REG_CPU_IPF|REG_CPU_IA32|TRACE_BASIC_API|BBL_BASIC_API|SYM_BASIC_API|MISC_PRINT|MISC_PARSE|KNOB_API|KNOB_BASIC|KNOB_PRINT|LOCK|PIN_CONTROL|PROTO_API|PIN_THREAD_API|DEBUG_API|TYPE_BASE|INSTLIB|ALARM|ROGUE|ROGUE_CFG|ROGUE_CFG_BASIC_API|ROGUE_EDG_BASIC_API|ROGUE_BBL_BASIC_API|ROGUE_INS_BASIC_API|ROGUE_TRACE_BASIC_API|ROGUE_CONTROL_BASIC_API|CODECACHE_API|SUPERPIN_API|OPTIMIZATION_API|MISC|CHECKPOINTING|PROTO|INST_ARGS|PIN_THREAD_PRIVATE|EDG_BASIC|BBL_BASIC|ROGUE_BASIC_API|CODECACHE_NYI_API|INS_BASIC_API_GEN_IPF|MESSAGE_TYPE|MESSAGE_BASIC|MISC_BASIC|ITC_INST_API|CHECKPOINTING_PRIVATE|UNDOCUMENTED_PIN_API|DEPRECATED_PIN_API
/* PIN API */

/* THIS FILE IS AUTOMAGICALLY GENERATED - DO NOT CHANGE DIRECTLY*/


enum
{
    // The [0, TLS_KEY_RESERVED_LAST) range is reserved for keys, that can be used
    // by glibc/libpinpthread without allocating them. Needed for Mac only?
    TLS_KEY_RESERVED_LAST = 140,

    TLS_KEY_CLIENT_FIRST,  
    // Keys in the [TLS_KEY_CLIENT_FIRST, TTLS_KEY_CLIENT_LAST] range can be 
    // allocated by tool
    TLS_KEY_CLIENT_LAST = TLS_KEY_CLIENT_FIRST + 63
};

                                                                  /* DO NOT EDIT */
const UINT32 MAX_CLIENT_TLS_KEYS = (TLS_KEY_CLIENT_LAST - TLS_KEY_CLIENT_FIRST + 1);

                                                                  /* DO NOT EDIT */
class CLIENT_TLS
{
  public:

    /*! @ingroup PIN_THREAD_PRIVATE
     *  Allocate a new TLS key and associate it with a given data destruction function.
     *  See PIN_CreateThreadDataKey() description.
     */
    TLS_KEY AllocateKey(DESTRUCTFUN destructFun = NULL);

    /*! @ingroup PIN_THREAD_PRIVATE
     *  Release TLS key, allocated by a previous call to the CreateThreadDataKey() function.
     *  See PIN_DeleteThreadDataKey() description.
     */
    BOOL FreeKey(TLS_KEY tlsKey);

    /*! @ingroup PIN_THREAD_PRIVATE
     *  Get/Set the value of the specified TLS slot of the current thread.
     *  See PIN_GetThreadData() and PIN_SetThreadData() description.
     *  @param[in] currentThreadId   ID of the current thread assigned by pin.
     *                               This ID can be obtained by the PIN_ThreadId() 
     *                               function call or received as the value of the 
     *                               IARG_THREAD_ID argument.
     *                               This optional parameter, if supplied, significantly 
     *                               improves performance of this function.
     * @note Fast variants of Get/SetData functions are defined as static to 
     *       eliminate overhead of the Instance() function. These functions assume
     *       that the Instance() function is invoked at least once before the 
     *       first call to these functions. This assumption is valid because we enforce
     *       Instance() invocation from the static initializer of the <m_pInstance> member.
     *       Notice, ptrace initialization may accesses TLS even earlier, but it never 
     *       uses these fast functions.
     */
    VOID *        GetData(TLS_KEY tlsKey) const;
    static VOID * GetData(TLS_KEY tlsKey, THREADID currentThreadId)
    {
        return (*(m_pSlots[currentThreadId]))[tlsKey];
    }

    /*! @ingroup PIN_THREAD_PRIVATE
     *  Get/Set the value of the specified TLS slot of the current thread.
     *  See above.
     */
    BOOL        SetData(TLS_KEY tlsKey, const VOID *  value);
    static BOOL SetData(TLS_KEY tlsKey, const VOID *  value, THREADID currentThreadId)
    {
        (*(m_pSlots[currentThreadId]))[tlsKey] = const_cast<VOID *>(value);
        return TRUE;
    }

    /*! @ingroup PIN_THREAD_PRIVATE
     *  Zero-initialize all TLS slots in the current thread.
     *  This function should be called on the thread start but before the client 
     *  is notified about the new thread.
     *  @param[in] currentThreadId   ID of the new (current) thread assigned by pin.
     */
    VOID InitCurrentTls(THREADID currentThreadId);

    /*! @ingroup PIN_THREAD_PRIVATE
     *  Destruct all per-thread data stored in TLS slots of the current thread.
     *  This function is called when the current thread is about to exit,
     *  just after notifying the client about thread exit.
     *  @param[in] currentThreadId   ID of the exiting (current) thread assigned by pin.
     */
    VOID DestructCurrentTls(THREADID currentThreadId);

    /*! @ingroup PIN_THREAD_PRIVATE
     *  Return single instance of this class.
     */
    static CLIENT_TLS * Instance();

  private:
    /*! @ingroup PIN_THREAD_PRIVATE
     *  Per-thread array of client's TLS slots indexed by TLS_KEY.
     */
    typedef TLS_ARRAY<0, TLS_KEY_CLIENT_LAST> SLOTS;

  private:
    // Private constructor
    CLIENT_TLS() {}
    friend class DUMMY_GCC32; //dummy friend to appease gcc 3.2.X compiler. It does not understand 
                              //singleton idiom and reports warning for private constructor.
    //Disable copy constructor and assignment operator
    CLIENT_TLS(const CLIENT_TLS &);
    CLIENT_TLS & operator = (const CLIENT_TLS &);

    static SLOTS * m_pSlots[PIN_MAX_THREADS];

    /*
     * Pointer to a single instance of this class
     * This static member enforces initialization of CLIENT_TLS in the first (main) thread.
     */
    static CLIENT_TLS * m_pInstance; 

};

                                                                  /* DO NOT EDIT */
inline TLS_KEY PIN_CreateThreadDataKey(DESTRUCTFUN destruct_func)
{
    return CLIENT_TLS::Instance()->AllocateKey(destruct_func);
}

                                                                  /* DO NOT EDIT */
inline BOOL PIN_DeleteThreadDataKey(TLS_KEY key)
{
    return CLIENT_TLS::Instance()->FreeKey(key);
}

                                                                  /* DO NOT EDIT */
inline BOOL PIN_SetThreadData( TLS_KEY key, const VOID * data )
{
    return CLIENT_TLS::Instance()->SetData( key, data );
}

                                                                  /* DO NOT EDIT */
inline BOOL PIN_SetThreadData( TLS_KEY key, const VOID * data , THREADID currentThreadId)
{
    return CLIENT_TLS::SetData( key, data, currentThreadId );
}

                                                                  /* DO NOT EDIT */
inline VOID * PIN_GetThreadData( TLS_KEY key )
{
    return CLIENT_TLS::Instance()->GetData( key );
}

                                                                  /* DO NOT EDIT */
inline VOID * PIN_GetThreadData( TLS_KEY key, THREADID currentThreadId )
{
    return CLIENT_TLS::GetData( key, currentThreadId );
}

                                                                  /* DO NOT EDIT */

