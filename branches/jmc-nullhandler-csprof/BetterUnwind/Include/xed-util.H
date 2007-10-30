/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2005 Intel Corporation 
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
/// @file xed-util.H 
/// @author Mark Charney   <mark.charney@intel.com> 



#ifndef _XED_UTIL_H_
# define _XED_UTIL_H_

#include "xed-common-hdrs.H"
#include "xed-types.H"

//include this to allow the existing users of xed-util.H to avoid changing.
#include "xed-func-util.H"

namespace XED {
using namespace std;
  
////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////
extern int xed_verbose;
#if XED_PRINT==1
extern  ostream* xed_log_ostream;
#endif
#define XED_EMIT_MESSAGES  (xed_verbose >= 1)
#define XED_INFO_VERBOSE   (xed_verbose >= 2)
#define XED_INFO2_VERBOSE  (xed_verbose >= 3)
#define XED_VERBOSE        (xed_verbose >= 4)
#define XED_MORE_VERBOSE   (xed_verbose >= 5)
#define XED_VERY_VERBOSE   (xed_verbose >= 6)

#if defined(__GNUC__)
# define XED_FUNCNAME __func__
#else
# define XED_FUNCNAME ""
#endif

#if XED_PRINT==1
#define XED_INTERNAL_MSGN(x)                                    \
    do {                                                        \
        if (XED_VERY_VERBOSE) {                                 \
            *xed_log_ostream << __FILE__ << ":" << __LINE__     \
                             << ":" << XED_FUNCNAME << ": " ;       \
        }                                                       \
        *xed_log_ostream << x ;                                 \
        xed_log_ostream->flush();                               \
    } while(0)

// for some crazy reason, I must have the if() statement or I get
// iostreams errors about copy constructors in gcc 3.4.x
#define XED_INTERNAL_MSG(x)                     \
    do {                                        \
        if (1)                                  \
        {                                       \
	  XED_INTERNAL_MSGN( x << std::endl );	\
        }                                       \
    } while(0)

#define XED_ERROR(x)                                                    \
    do { if (XED_EMIT_MESSAGES) { XED_INTERNAL_MSG( "XED ERROR: " << x ); } } while(0)

#define XED_WARN(x)                                                     \
    do { if (XED_EMIT_MESSAGES) { XED_INTERNAL_MSG( "XED WARNING: " << x ); } } while(0)


#else
# define XED_ERROR(x)
# define XED_WARN(x) 
# define XED_MSGN(x) 
# define XED_MSG(x)  
# define XED_INTERNAL_MSG(x)
# define XED_INTERNAL_MSGN(x)
#endif

#if defined(XED_MESSAGES)

# define XED_MSGN(x)   XED_INTERNAL_MSGN(x)
# define XED_MSG(x)    XED_INTERNAL_MSG(x)

# define XTMSGN(x)                              \
    do {                                        \
        if (XED_VERBOSE) {                      \
            XED_MSGN( x ) ;                     \
        }                                       \
    } while(0)

# define XTMSG(x)   XTMSGN(x << std::endl) 

#define XMMSGN(x)                               \
    do {                                        \
        if (XED_MORE_VERBOSE) {                 \
            XED_MSGN( x ) ;                     \
        }                                       \
    } while(0)

#define XMMSG(x)   XMMSGN(x << std::endl) 

#define XIMSGN(x)                               \
    do {                                        \
        if (XED_INFO_VERBOSE) {                 \
            XED_MSGN( x );                      \
        }                                       \
    } while(0)

#define XIMSG(x)   XIMSGN(x << std::endl) 

#define XJMSGN(x)                               \
    do {                                        \
        if (XED_INFO2_VERBOSE) {                \
            XED_MSGN( x );                      \
        }                                       \
    } while(0)

#define XJMSG(x)   XJMSGN(x << std::endl) 


#define XVMSG(x)                                \
    do {                                        \
        if (XED_VERY_VERBOSE) {                 \
	  XTMSGN(x << std::endl) ;		\
        }                                       \
    } while(0)


#else
// placeholders for when messages are compiled out.
# define XED_MSGN(x)   
# define XED_MSG(x)    
# define XTMSGN(x)   
# define XTMSG(x)    
# define XMMSGN(x)   
# define XMMSG(x)    
# define XJMSG(x)
# define XJMSGN(x)
# define XIMSGN(x)   
# define XIMSG(x)    
# define XVMSG(x)    

#endif

////////////////////////////////////////////////////////////////////////////

#define NDY(x)                                  \
    do {                                        \
        XED_ERROR( "NOT DONE YET: " << x) ;	\
        assert(0);                              \
    } while(0)

#define XED_FATAL(x)                            \
    do {                                        \
        XED_ERROR( "XED FATAL ERROR: " << x) ;	\
        assert(0);                              \
    } while(0)

} // namespace

////////////////////////////////////////////////////////////////////////////
#endif
//Local Variables:
//pref: "../../xed-util.cpp"
//End:
