/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2007 Intel Corporation 
All rights reserved. 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/// @file xed-util.h 
/// @author Mark Charney   <mark.charney@intel.com> 



#ifndef _XED_UTIL_H_
# define _XED_UTIL_H_

#include "xed-common-hdrs.h"
#include "xed-types.h"
#include "xed-portability.h"

  
////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////
extern int xed_verbose;
#if defined(XED_MESSAGES)
# include <stdio.h> // only with XED_MESSAGES defined
extern  FILE* xed_log_file;
#endif
#define XED_EMIT_MESSAGES  (XED_MESSAGES==1 && xed_verbose >= 1)
#define XED_INFO_VERBOSE   (XED_MESSAGES==1 && xed_verbose >= 2)
#define XED_INFO2_VERBOSE  (XED_MESSAGES==1 && xed_verbose >= 3)
#define XED_VERBOSE        (XED_MESSAGES==1 && xed_verbose >= 4)
#define XED_MORE_VERBOSE   (XED_MESSAGES==1 && xed_verbose >= 5)
#define XED_VERY_VERBOSE   (XED_MESSAGES==1 && xed_verbose >= 6)

#if defined(__GNUC__)
# define XED_FUNCNAME __func__
#else
# define XED_FUNCNAME ""
#endif

#if XED_MESSAGES==1
#define XED2IMSG(x)                                             \
    do {                                                        \
        if (XED_EMIT_MESSAGES) {                                \
            if (XED_VERY_VERBOSE) {                             \
                fprintf(xed_log_file,"%s:%d:%s: ",              \
                        __FILE__, __LINE__, XED_FUNCNAME);      \
            }                                                   \
            fprintf x;                                          \
            fflush(xed_log_file);                               \
        }                                                       \
    } while(0)

#define XED2TMSG(x)                                             \
    do {                                                        \
        if (XED_VERBOSE) {                                      \
            if (XED_VERY_VERBOSE) {                             \
                fprintf(xed_log_file,"%s:%d:%s: ",              \
                        __FILE__, __LINE__, XED_FUNCNAME);      \
            }                                                   \
            fprintf x;                                          \
            fflush(xed_log_file);                               \
        }                                                       \
    } while(0)

#define XED2VMSG(x)                                             \
    do {                                                        \
        if (XED_VERY_VERBOSE) {                                 \
            fprintf(xed_log_file,"%s:%d:%s: ",                  \
                    __FILE__, __LINE__, XED_FUNCNAME);          \
            fprintf x;                                          \
            fflush(xed_log_file);                               \
        }                                                       \
    } while(0)

#define XED2DIE(x)                                              \
    do {                                                        \
        if (XED_EMIT_MESSAGES) {                                \
            fprintf(xed_log_file,"%s:%d:%s: ",                  \
                             __FILE__, __LINE__, XED_FUNCNAME); \
            fprintf x;                                          \
            fflush(xed_log_file);                               \
        }                                                       \
        xed_assert(0);                                          \
    } while(0)



#else
# define XED2IMSG(x) 
# define XED2TMSG(x)
# define XED2VMSG(x)
# define XED2DIE(x) do { xed_assert(0); } while(0)
#endif

#if defined(XED_ASSERTS)
#  define xed_assert(x)  do { if (( x )== 0) xed_internal_assert( #x, __FILE__, __LINE__); } while(0) 
#else
#  define xed_assert(x)  do {  } while(0) 
#endif
XED_NORETURN XED_NOINLINE XED_DLL_EXPORT void xed_internal_assert(const char* s, const char* file, int line);

/// @ingroup INIT
/// This is for registering a function to be called during XED's assert
/// processing. If you do not register an abort function, then the system's
/// abort function will be called. If your supplied function returns, then
/// abort() will still be called.
///
/// @param fn This is a function pointer for a function that should handle the
///        assertion reporting. The function pointer points to  a function that
///        takes 4 arguments: 
///                     (1) msg, the assertion message, 
///                     (2) file, the file name,
///                     (3) line, the line number (as an integer), and
///                     (4) other, a void pointer that is supplied as thei
///                         2nd argument to this registration.
/// @param other This is a void* that is passed back to your supplied function  fn
///        as its 4th argument. It can be zero if you don't need this
///        feature. You can used this to convey whatever additional context
///        to your assertion handler (like FILE* pointers etc.).
///
XED_DLL_EXPORT void xed_register_abort_function(void (*fn)(const char* msg,
                                                           const char* file, int line, void* other),
                                                void* other);


////////////////////////////////////////////////////////////////////////////
// PROTOTYPES
////////////////////////////////////////////////////////////////////////////
char* xed_downcase_buf(char* s);

int xed_itoa(char* buf, uint64_t f, int buflen);
int xed_itoa_hex_zeros(char* buf, uint64_t f, uint_t bits_to_print, bool leading_zeros, int buflen);
int xed_itoa_hex(char* buf, uint64_t f, uint_t bits_to_print, int buflen);
int xed_itoa_signed(char* buf, int64_t f, int buflen);

char xed_to_ascii_hex_nibble(uint_t x);

int xed_sprintf_uint8_hex(char* buf, uint8_t x, int buflen);
int xed_sprintf_uint16_hex(char* buf, uint16_t x, int buflen);
int xed_sprintf_uint32_hex(char* buf, uint32_t x, int buflen);
int xed_sprintf_uint64_hex(char* buf, uint64_t x, int buflen);
int xed_sprintf_uint8(char* buf, uint8_t x, int buflen);
int xed_sprintf_uint16(char* buf, uint16_t x, int buflen);
int xed_sprintf_uint32(char* buf, uint32_t x, int buflen);
int xed_sprintf_uint64(char* buf, uint64_t x, int buflen);
int xed_sprintf_int8(char* buf, int8_t x, int buflen);
int xed_sprintf_int16(char* buf, int16_t x, int buflen);
int xed_sprintf_int32(char* buf, int32_t x, int buflen);
int xed_sprintf_int64(char* buf, int64_t x, int buflen);


/// Set the FILE* for XED's log msgs
XED_DLL_EXPORT void xed_set_log_file(FILE* o);


/// Set the verbosity level for XED
XED_DLL_EXPORT void xed_set_verbosity(int v);

void xed_derror(const char* s);
void xed_dwarn(const char* s);

XED_DLL_EXPORT int64_t xed_sign_extend32_64(int32_t x);
XED_DLL_EXPORT int64_t xed_sign_extend16_64(int16_t x);
XED_DLL_EXPORT int64_t xed_sign_extend8_64(int8_t x);

XED_DLL_EXPORT int32_t xed_sign_extend16_32(int16_t x);
XED_DLL_EXPORT int32_t xed_sign_extend8_32(int8_t x);

XED_DLL_EXPORT int16_t xed_sign_extend8_16(int8_t x);

///arbitrary sign extension from a qty of "bits" length to 32b 
XED_DLL_EXPORT int32_t xed_sign_extend_arbitrary_to_32(uint32_t x, unsigned int bits);

///arbitrary sign extension from a qty of "bits" length to 64b 
XED_DLL_EXPORT int64_t xed_sign_extend_arbitrary_to_64(uint64_t x, unsigned int bits);


XED_DLL_EXPORT uint64_t xed_zero_extend32_64(uint32_t x);
XED_DLL_EXPORT uint64_t xed_zero_extend16_64(uint16_t x);
XED_DLL_EXPORT uint64_t xed_zero_extend8_64(uint8_t x);

XED_DLL_EXPORT uint32_t xed_zero_extend16_32(uint16_t x);
XED_DLL_EXPORT uint32_t xed_zero_extend8_32(uint8_t x);

XED_DLL_EXPORT uint16_t xed_zero_extend8_16(uint8_t x);

XED_DLL_EXPORT int32_t 
xed_little_endian_to_int32(uint64_t x, unsigned int len);

XED_DLL_EXPORT int64_t 
xed_little_endian_to_int64(uint64_t x, unsigned int len);
XED_DLL_EXPORT uint64_t 
xed_little_endian_to_uint64(uint64_t x, unsigned int len);

XED_DLL_EXPORT int64_t 
xed_little_endian_hilo_to_int64(uint32_t hi_le, uint32_t lo_le, unsigned int len);
XED_DLL_EXPORT uint64_t 
xed_little_endian_hilo_to_uint64(uint32_t hi_le, uint32_t lo_le, unsigned int len);

XED_DLL_EXPORT uint8_t
xed_get_byte(uint64_t x, unsigned int i, unsigned int len);

static XED_INLINE uint64_t xed_make_uint64(uint32_t hi, uint32_t lo) {
    uint64_t x,y;
    x=hi;
    y= (x<<32) | lo;
    return y;
}
static XED_INLINE int64_t xed_make_int64(uint32_t hi, uint32_t lo) {
    uint64_t x,y;
    x=hi;
    y= (x<<32) | lo;
    return STATIC_CAST(int64_t,y);
}

/// returns the number of bytes required to store the UNSIGNED number x
/// given a mask of legal lengths. For the legal_widths argument, bit 0
/// implies 1 byte is a legal return width, bit 1 implies that 2 bytes is a
/// legal return width, bit 2 implies that 4 bytes is a legal return width.
/// This returns 8 (indicating 8B) if none of the provided legal widths
/// applies.
XED_DLL_EXPORT uint_t xed_shortest_width_unsigned(uint64_t x, uint8_t legal_widths);

/// returns the number of bytes required to store the SIGNED number x
/// given a mask of legal lengths. For the legal_widths argument, bit 0 implies 1
/// byte is a legal return width, bit 1 implies that 2 bytes is a legal
/// return width, bit 2 implies that 4 bytes is a legal return width.  This
/// returns 8 (indicating 8B) if none of the provided legal widths applies.
XED_DLL_EXPORT uint_t xed_shortest_width_signed(int64_t x, uint8_t legal_widths);

////////////////////////////////////////////////////////////////////////////
// GLOBALS
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
#endif
//Local Variables:
//pref: "../../xed-util.c"
//End:
