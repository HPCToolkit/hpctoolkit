//Groups: @ingroup\s+(API_REF|IMG_BASIC_API|INS_BASIC_API|INS_INST_API|INS_BASIC_API_GEN_IA32|INS_BASIC_API_IA32|INS_BASIC_API_IPF|INS_MOD_API_GEN_IA32|SEC_BASIC_API|RTN_BASIC_API|REG_BASIC_API|REG_CPU_GENERIC|REG_CPU_IPF|REG_CPU_IA32|TRACE_BASIC_API|BBL_BASIC_API|SYM_BASIC_API|MISC_PRINT|MISC_PARSE|KNOB_API|KNOB_BASIC|KNOB_PRINT|LOCK|PIN_CONTROL|PROTO_API|PIN_THREAD_API|DEBUG_API|TYPE_BASE|INSTLIB|ALARM|ROGUE|ROGUE_CFG|ROGUE_CFG_BASIC_API|ROGUE_EDG_BASIC_API|ROGUE_BBL_BASIC_API|ROGUE_INS_BASIC_API|ROGUE_TRACE_BASIC_API|ROGUE_CONTROL_BASIC_API|CODECACHE_API|SUPERPIN_API|OPTIMIZATION_API|MISC|CHECKPOINTING|PROTO|INST_ARGS|PIN_THREAD_PRIVATE|EDG_BASIC|BBL_BASIC|ROGUE_BASIC_API|CODECACHE_NYI_API|INS_BASIC_API_GEN_IPF|MESSAGE_TYPE|MESSAGE_BASIC|MISC_BASIC|ITC_INST_API|CHECKPOINTING_PRIVATE|UNDOCUMENTED_PIN_API|DEPRECATED_PIN_API
/* PIN API */

/* THIS FILE IS AUTOMAGICALLY GENERATED - DO NOT CHANGE DIRECTLY*/


extern VOID SetAddress0x(BOOL val);

                                                                  /* DO NOT EDIT */
extern BOOL CharIsSpace(CHAR c);

                                                                  /* DO NOT EDIT */
extern CHAR CharToUpper(CHAR c);

                                                                  /* DO NOT EDIT */
extern string ptrstr(const VOID *val );

                                                                  /* DO NOT EDIT */
extern string rjstr( const string& s, UINT32 width, CHAR padding);

                                                                  /* DO NOT EDIT */
extern string StringFromAddrint( ADDRINT l);

                                                                  /* DO NOT EDIT */
extern string StringDec( UINT64 l, UINT32 digits, CHAR padding);

                                                                  /* DO NOT EDIT */
extern string StringDecSigned( INT64 l, UINT32 digits, CHAR padding);

                                                                  /* DO NOT EDIT */
extern string decstrarray(UINT32 *vals, UINT32 num);

                                                                  /* DO NOT EDIT */
extern string StringBignum( INT64 l, UINT32 digits, CHAR padding);

                                                                  /* DO NOT EDIT */
extern string Reformat(const string& s, const string& prefix, UINT32 min_line, UINT32 max_line);

                                                                  /* DO NOT EDIT */
extern string StringHex32( UINT32 l, UINT32 digits, BOOL prefix_0x);

                                                                  /* DO NOT EDIT */
extern string StringFlt(FLT64 val , UINT32 precision, UINT32 width);

                                                                  /* DO NOT EDIT */
extern string StringBool(BOOL b);

                                                                  /* DO NOT EDIT */
extern string StringTri(TRI t);

                                                                  /* DO NOT EDIT */
extern UINT32 Uint32FromString(const string& s);

                                                                  /* DO NOT EDIT */
extern UINT64 Uint64FromString(const string& s);

                                                                  /* DO NOT EDIT */
extern FLT64 FLT64FromString(const string& s);

                                                                  /* DO NOT EDIT */
extern ADDRINT AddrintFromString(const string& str);

                                                                  /* DO NOT EDIT */
extern UINT32 BitCount(UINT32 val);

                                                                  /* DO NOT EDIT */
extern string ReadLine(istream& inputFile, UINT32 *lineNum);

                                                                  /* DO NOT EDIT */
extern UINT32 Tokenize(const string& line, string *array, UINT32 n);

                                                                  /* DO NOT EDIT */
inline string StringHex( UINT32 l, UINT32 digits, BOOL prefix_0x = TRUE) {return StringHex32(l, digits, prefix_0x);}

                                                                  /* DO NOT EDIT */
inline string decstr(INT64 val, UINT32 width=0 )  {return StringDecSigned(val,width,' ');}

                                                                  /* DO NOT EDIT */
inline string decstr(INT32 val, UINT32 width=0 )  {return StringDecSigned(val,width,' ');}

                                                                  /* DO NOT EDIT */
inline string decstr(INT16 val, UINT32 width=0 )  {return StringDecSigned(val,width,' ');}

                                                                  /* DO NOT EDIT */
inline string decstr(UINT64 val, UINT32 width=0 )  {return StringDec(val,width,' ');}

                                                                  /* DO NOT EDIT */
inline string decstr(UINT32 val, UINT32 width=0 )  {return StringDec(val,width,' ');}

                                                                  /* DO NOT EDIT */
inline string decstr(UINT16 val, UINT32 width=0 )  {return StringDec(val,width,' ');}

                                                                  /* DO NOT EDIT */
inline string hexstr(INT64 val, UINT32 width=0 )
{
    string ostr;
#if defined(_MSC_VER) && _MSC_VER  >= 1400
    ostr = StringHex( INT32((val >> 16) >> 16), width);
#else
    ostr = StringHex( INT32(val >> 32), width);
#endif
    ostr += StringHex( UINT32(val), 8, FALSE);

    return ostr;
}

                                                                  /* DO NOT EDIT */
inline string hexstr(INT32 val, UINT32 width=0 )  {return StringHex(INT32(val),width);}

                                                                  /* DO NOT EDIT */
inline string hexstr(INT16 val, UINT32 width=0 )  {return StringHex(INT32(val),width);}

                                                                  /* DO NOT EDIT */
inline string hexstr(UINT64 val, UINT32 width=0 )
{
    string ostr;
#if defined(_MSC_VER) && _MSC_VER  >= 1400
    ostr = StringHex( UINT32((val >> 16) >> 16), width);
#else
    ostr = StringHex( UINT32(val >> 32), width);
#endif
    ostr += StringHex( UINT32(val), 8, FALSE);

    return ostr;
}

                                                                  /* DO NOT EDIT */
inline string hexstr(VOID *p, UINT32 width=0 )
{
#if defined(HOST_IPF) || defined(HOST_IA32E)
    UINT64 val = reinterpret_cast<UINT64>(p);
    return hexstr(val,width);
#else
    UINT32 val = reinterpret_cast<UINT32>(p);
    return StringHex(val,width);
#endif    
}

                                                                  /* DO NOT EDIT */
inline string hexstr(const VOID *p, UINT32 width=0 )
{
#if defined(HOST_IPF) || defined(HOST_IA32E)
    UINT64 val = reinterpret_cast<UINT64>(p);
    return hexstr(val,width);
#else
    UINT32 val = reinterpret_cast<UINT32>(p);
    return StringHex(val,width);
#endif    
}

                                                                  /* DO NOT EDIT */
inline string hexstr(UINT32 val, UINT32 width=0 )  {return StringHex(UINT32(val),width);}

                                                                  /* DO NOT EDIT */
inline string hexstr(UINT16 val, UINT32 width=0 )  {return StringHex(UINT32(val),width);}

                                                                  /* DO NOT EDIT */
inline string fltstr(FLT64 val, UINT32 prec=0,UINT32 width=0 )  {return StringFlt(val,prec,width);}

                                                                  /* DO NOT EDIT */
inline string ljstr(const string& s, UINT32 width, CHAR padding = ' ')
{
    string  ostr(width,padding);
    ostr.replace(0,s.length(),s);
    return ostr;
}

                                                                  /* DO NOT EDIT */
inline VOID * Addrint2VoidStar(ADDRINT addr)
{
#if (defined(HOST_IA32E) || defined(HOST_IPF)) && (defined(TARGET_IA32))
    ASSERT(false,"Should not be called in cross environment\n");
    return 0;
#else
    return reinterpret_cast<VOID*>(addr);
#endif    
}

                                                                  /* DO NOT EDIT */
inline ADDRINT VoidStar2Addrint(const VOID * addr)
{
#if (defined(HOST_IA32E) || defined(HOST_IPF)) && (defined(TARGET_IA32))
    ASSERT(false,"Should not be called in cross environment\n");
    return 0;
#else
    return reinterpret_cast<ADDRINT>(addr);
#endif
}

                                                                  /* DO NOT EDIT */
inline ADDRINT VoidStar2Addrint(VOID * addr)
{
#if (defined(HOST_IA32E) || defined(HOST_IPF)) && (defined(TARGET_IA32))
    ASSERT(false,"Should not be called in cross environment\n");
    return 0;
#else
    return reinterpret_cast<ADDRINT>(addr);
#endif
}

                                                                  /* DO NOT EDIT */
inline ADDRINT RoundUp(ADDRINT value, UINT32 alignment)
{
    if (alignment == 0)
    {
        return value;
    }
    
    value += alignment - 1;
    value /= alignment;
    value *= alignment;

    return value;
}

                                                                  /* DO NOT EDIT */
inline ADDRINT RoundUpPowerOf2(ADDRINT value, UINT32 alignment)
{
    ASSERTX( ((((alignment-1) & (alignment)) == 0) && (0 != alignment)));
    ADDRINT alignmentMask = alignment - 1;
    value += alignmentMask;
    value &= ~alignmentMask;

    return value;
}

                                                                  /* DO NOT EDIT */
inline ADDRINT RoundDown(ADDRINT value, UINT32 alignment)
{
    if (alignment == 0)
    {
        return value;
    }

    value /= alignment;
    value *= alignment;

    return value;
}

                                                                  /* DO NOT EDIT */
inline ADDRINT RoundDownPowerOf2(ADDRINT value, UINT32 alignment)
{
    ASSERTX( ((((alignment-1) & (alignment)) == 0) && (0 != alignment)));
    ADDRINT alignmentMask = alignment - 1;
    return ( value & ~alignmentMask );
}

                                                                  /* DO NOT EDIT */
inline VOID * RoundUp(VOID * address, UINT32 alignment)
{
    return Addrint2VoidStar(RoundUp(VoidStar2Addrint(address), alignment));
}

                                                                  /* DO NOT EDIT */
inline VOID * RoundUpPowerOf2(VOID * address, UINT32 alignment)
{
    return Addrint2VoidStar(RoundUpPowerOf2(VoidStar2Addrint(address), alignment));
}

                                                                  /* DO NOT EDIT */
inline VOID * RoundDown(VOID * address, UINT32 alignment)
{
    return Addrint2VoidStar(RoundDown(VoidStar2Addrint(address), alignment));
}

                                                                  /* DO NOT EDIT */
inline VOID * RoundDownPowerOf2(VOID * address, UINT32 alignment)
{
    return Addrint2VoidStar(RoundDownPowerOf2(VoidStar2Addrint(address), alignment));
}

                                                                  /* DO NOT EDIT */
template<typename T> ADDRINT PointerToInt(const T *ptr)
{
#if (defined(HOST_IA32) && !defined(TARGET_IA32)) || (defined(HOST_IA32E) && !defined(TARGET_IA32E)) || \
    (defined(HOST_IPF) && !defined(TARGET_IPF))

    ASSERT(false,"Should not be called in cross environment\n");
    return 0;

#else
    return reinterpret_cast<ADDRINT>(ptr);
#endif
}

                                                                  /* DO NOT EDIT */
template<typename T> T *IntToPointer(ADDRINT addr)
{
#if (defined(HOST_IA32) && !defined(TARGET_IA32)) || (defined(HOST_IA32E) && !defined(TARGET_IA32E)) || \
    (defined(HOST_IPF) && !defined(TARGET_IPF))

    ASSERT(false,"Should not be called in cross environment\n");
    return 0;

#else
    return reinterpret_cast<T*>(addr);
#endif
}

                                                                  /* DO NOT EDIT */
extern int VoidStar2Int(VOID * addr);

                                                                  /* DO NOT EDIT */
extern void RangeCheck(const UINT32 value, const UINT32 bits);

                                                                  /* DO NOT EDIT */
extern BOOL UnsignedImmediateFits(const UINT64 value, const UINT32 bits);

                                                                  /* DO NOT EDIT */
extern BOOL SignedImmediateFits(const INT64 value, const UINT32 origSize, const UINT32 extendedSize);

                                                                  /* DO NOT EDIT */
extern BOOL SignedImmediateFits(const INT64 value, const UINT32 origSize);

                                                                  /* DO NOT EDIT */
extern BOOL ExtractArgumentsPin(INT32 argc, CHAR **argv, INT32 * pargc, CHAR ***pargv);

                                                                  /* DO NOT EDIT */
extern BOOL ExtractArgumentsTool(INT32 argc, CHAR **argv, INT32 * targc, CHAR ***targv);

                                                                  /* DO NOT EDIT */
extern VOID GetArgumentsApp(INT32 * aargc, CHAR ***aargv);

                                                                  /* DO NOT EDIT */
extern BOOL ExtractArgumentsApp(INT32 argc, CHAR ** argv, INT32 * aargc, CHAR ***aargv);

                                                                  /* DO NOT EDIT */
extern INT32 MEMORY_ReadInt32(ADDRINT  address);

                                                                  /* DO NOT EDIT */
extern VOID MEMORY_WriteInt32(ADDRINT  address, INT32 value);

                                                                  /* DO NOT EDIT */
extern UINT32 MEMORY_ReadUint32(ADDRINT  address);

                                                                  /* DO NOT EDIT */
extern VOID MEMORY_WriteUint32(ADDRINT  address, UINT32 value);

                                                                  /* DO NOT EDIT */
extern INT64 MEMORY_ReadInt64(ADDRINT  address);

                                                                  /* DO NOT EDIT */
extern VOID MEMORY_WriteInt64(ADDRINT  address, INT64 value);

                                                                  /* DO NOT EDIT */
extern UINT64 MEMORY_ReadUint64(ADDRINT  address);

                                                                  /* DO NOT EDIT */
extern VOID MEMORY_WriteUint64(ADDRINT  address, UINT32 value);

                                                                  /* DO NOT EDIT */
extern ADDRINT GetPageOfAddr(ADDRINT addr);

                                                                  /* DO NOT EDIT */
extern ADDRINT ProgramEntry(const ImageLoaderInfo * mainImage, const ImageLoaderInfo * loader);

                                                                  /* DO NOT EDIT */
extern string Joinpath(string s1, string s2);

                                                                  /* DO NOT EDIT */
extern string Basename(const string& path);

                                                                  /* DO NOT EDIT */
extern CHAR* CreateTmpFileName(CHAR* fnameTemplate);

                                                                  /* DO NOT EDIT */
extern const VOID * GetSp();

                                                                  /* DO NOT EDIT */
template <typename T> VOID * 
 PushDataToStack(VOID * stack, const T * data, size_t alignment = 1, size_t size = sizeof(T))
{
    stack = reinterpret_cast<VOID *>(reinterpret_cast<size_t>(stack) - size);
    stack = RoundDown(stack , alignment);
    return memcpy(stack, reinterpret_cast<const VOID *>(data), size);
}

                                                                  /* DO NOT EDIT */
inline size_t PtrDiff(const VOID * ptr1, const VOID * ptr2)
{
    return (reinterpret_cast<const INT8 *>(ptr1) - reinterpret_cast<const INT8 *>(ptr2));
}

                                                                  /* DO NOT EDIT */
inline VOID * PtrAtOffset(VOID * ptr, size_t offset)
{
    return (reinterpret_cast<INT8 *>(ptr) + offset);
}

                                                                  /* DO NOT EDIT */
class MemRange
{ 
    public:

    //======= Constructors

    //Construct an empty range
    MemRange(): m_base(0), m_size(0) {} 
    //Construct range with a given base address and size
    MemRange(VOID * base, size_t size) : m_base(base), m_size(size) {}
    //Construct range with a given base and end addresses.
    //End address of a range is the address of the first byte following the range
    MemRange(VOID * base, VOID * end) : m_base(base), m_size(PtrDiff(end, base)) {}

    //Default copy costructor and assignment operator

    //======= operators == and !=
    BOOL operator == (const MemRange &  range) const 
    {
        return ((m_base == range.m_base) && (m_size == range.m_size));
    }
    BOOL operator != (const MemRange &  range) const {return (!(*this == range));}

    //======= Accessors 

    //Get/set base address of the range. 
    VOID * Base() const {return m_base;}
    MemRange& Base(VOID * base) { m_base = base; return *this;}

    //Get/set size, in bytes, of the range. 
    size_t Size() const {return  m_size;}
    MemRange& Size(size_t size) { m_size = size; return *this;}

    //Get end address of the range. 
    VOID * End() const {return  PtrAtOffset(m_base, m_size);}

    /*
    * Check to see whether this range is empty
    *
    *  return  true, if this range is empty, otherwise - false
    */
    BOOL IsEmpty() const {return (Size() == 0);}

    /*
    * Check to see whether this range contains the specified memory address
    *
    *  param    addr[in] memory address, potentially contained in this range 
    *
    *  return   true, if this range contains the specified address, 
    *           otherwise - false 
    */
    BOOL Contains(const VOID * addr) const
    {
        return ((VoidStar2Addrint(addr) - VoidStar2Addrint(m_base)) < m_size);
    }

    /*
    * Check to see whether this range contains the specified memory range
    *
    *  param    range[in] memory range, potentially contained in this range 
    *
    *  return   true, if this range contains the specified range,
    *           otherwise - false
    */
    BOOL Contains(const MemRange & range) const
    {
        return  (Contains(range.m_base)&& 
                ((VoidStar2Addrint(range.m_base) - VoidStar2Addrint(m_base)) <= 
                (m_size - range.m_size)));
    }

    /* Check to see whether this range intersects with the specified memory range
    *
    *  param    range[in] memory range, potentially intersecting with this range 
    *
    *  return   true, if range intersects with the specified range
    *           otherwise - false
    */
    BOOL Intersects(const MemRange & range) const
    {
        return  (Contains(range.m_base) || range.Contains(m_base)); 
    }

    /* Check to see whether the current stack pointer belongs to this range 
    *
    *  return   true, if current stack pointer belongs to this range
    *           otherwise - false
    */
    BOOL IsCurrentStack() const
    {
        return  (Contains(GetSp())); 
    }

    /*
    * Round base/end address of the range down/up according to specified alignment
    *
    *  param    alignment[in] alignment value. Must be power of two. 
    *
    *  return  reference to aligned range
    */
    MemRange& Align(UINT32 alignment) 
    {
        VOID * end  = RoundUp(End(), alignment);
        m_base = RoundDown(m_base, alignment);
        m_size = PtrDiff(end, m_base);
        return *this;
    }

    /*
    * Round both base and end address of the range up according to specified alignment.
    * The size of the updated range is at least size the range before update.
    *  param    alignment[in] alignment value. Must be power of two. 
    *  return  reference to aligned range
    */
    MemRange& AlignUp(UINT32 alignment) 
    {
        m_base = RoundUp(m_base, alignment);
        m_size = RoundUp(m_size, alignment);
        return *this;
    }

    /*
    * Round both base and end address of the range down according to specified alignment.
    * The size of the updated range is at least size the range before update.
    *  param    alignment[in] alignment value. Must be power of two. 
    *  return  reference to aligned range
    */
    MemRange& AlignDown(UINT32 alignment) 
    {
        VOID * end  = RoundDown(End(), alignment);
        m_size = RoundUp(m_size, alignment);
        m_base = PtrAtOffset(end, 0 - m_size);
        return *this;
    }

    /*
    * Shift both base and end address of the range 
    *  param    offset[in] shift value. 
    *  return  reference to shifted range
    */
    MemRange& Shift(size_t offset) 
    {
        m_base = PtrAtOffset(m_base, offset);
        return *this;
    }

    /*
    * Shift base address of the range without changing the end address
    *  param    offset[in] shift value. 
    *  return  reference to shifted range
    */
    MemRange& ShiftBase(size_t offset) 
    {
        VOID * end  = End();
        m_base = PtrAtOffset(m_base, offset);
        m_size = PtrDiff(end, m_base);
        return *this;
    }

    /*
    * Shift end address of the range without changing the base address
    *  param    offset[in] shift value. 
    *  return  reference to shifted range
    */
    MemRange& ShiftEnd(size_t offset) 
    {
        VOID * end  = PtrAtOffset(End(), offset);
        m_size = PtrDiff(end, m_base);
        return *this;
    }

    private:
    VOID * m_base;
    size_t m_size;
};

                                                                  /* DO NOT EDIT */
class ONCE_STATE
{
  private:
    // Function execution state
    enum 
    {
        NO_ONCE,            //!< initial state - function never executed
        ONCE_RUNNING,       //!< function is currently running
        ONCE_SUCCESS,       //!< function executed successfully
        ONCE_FAILURE        //!< function executed but failed
    } m_state;

  public:
    // Initial state - never executed
    ONCE_STATE() : m_state(NO_ONCE) {}

    // Return FALSE if function is already executed. Otherwise, set state to ONCE_RUNNING
    // and return TRUE. 
    BOOL Enter() 
    {
        if (m_state == NO_ONCE)
        {
            m_state = ONCE_RUNNING; 
            return TRUE;
        }
        return FALSE;
    }

    // Set and return exit state of the function just executed
    BOOL Exit(BOOL result = TRUE) 
    {
        m_state = (result ? ONCE_SUCCESS : ONCE_FAILURE);
        return result;
    }

    // Return TRUE if function is already executed successfully
    BOOL IsSuccess() const {return (m_state == ONCE_SUCCESS); }
    // Return TRUE if function is already executed but failed
    BOOL IsFailure() const {return (m_state == ONCE_FAILURE); }
    // Return TRUE if function is already executed
    BOOL IsDone() const {return (IsSuccess() || IsFailure()); }

};

                                                                  /* DO NOT EDIT */
extern STAT_NORM StatRawMmapBytes;

                                                                  /* DO NOT EDIT */
extern STAT_NORM StatReservedBytes;

                                                                  /* DO NOT EDIT */
extern VOID UpdateRawMmapBytes();

                                                                  /* DO NOT EDIT */

