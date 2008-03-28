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
/// @file xed-interface.h 
/// @author  Mark Charney   <mark.charney@intel.com> 




#if !defined(_XED_INTERFACE_H_)
# define _XED_INTERFACE_H_

////////////////////////////////////////////////////////////////////////////
#include "xed-common-hdrs.h"
#include "xed-types.h"
//#include "xed-error-enum.h"
#include "xed-operand-enum.h"

#include "xed-init.h"
#include "xed-decode.h"

#include "xed-state.h" // dstate, legacy
#include "xed-syntax-enum.h"
#include "xed-reg-class-enum.h" // generated
#include "xed-reg-class.h"
#include "xed-inst-printer.h"

#include "xed-encode.h"
#include "xed-util.h"
//#include "xed-decode-cache.h"
#include "xed-inst-printer.h"
#include "xed-operand-action.h"

#include "xed-version.h"
#include "xed-decoded-inst.h"
#include "xed-inst.h"
#include "xed-iclass-enum.h" // generated
#include "xed-category-enum.h" // generated
#include "xed-extension-enum.h" // generated
#include "xed-attribute-enum.h" // generated
////////////////////////////////////////////////////////////////////////////

#endif
