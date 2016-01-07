// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//    ISA.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
using std::ostream;

#include <cstdarg>

#include <cstring>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>

#include "ISA.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

//****************************************************************************

//****************************************************************************
// ISA
//****************************************************************************

ISA::ISA()
  : refcount(1)
{
  _cache = new DecodingCache();
}


ISA::~ISA()
{
  delete _cache;
}


//****************************************************************************
// ISA::InstDesc
//****************************************************************************

const char*
ISA::InsnDesc::toString() const
{
  switch(ty) {
  case MEM_LOAD:        return "MEM_LOAD";
  case MEM_STORE:       return "MEM_STORE";
  case MEM_OTHER:       return "MEM_OTHER";

  case INT_BR_COND_REL: return "INT_BR_COND_REL";
  case INT_BR_COND_IND: return "INT_BR_COND_IND";
  case FP_BR_COND_REL:  return "FP_BR_COND_REL";
  case FP_BR_COND_IND:  return "FP_BR_COND_IND";

  case BR_UN_COND_REL:  return "BR_UN_COND_REL";
  case BR_UN_COND_IND:  return "BR_UN_COND_IND";

  case SUBR_REL:        return "SUBR_REL";
  case SUBR_IND:        return "SUBR_IND";
  case SUBR_RET:        return "SUBR_RET";

  case INT_ADD:         return "INT_ADD";
  case INT_SUB:         return "INT_SUB";
  case INT_MUL:         return "INT_MUL";
  case INT_CMP:         return "INT_CMP";
  case INT_LOGIC:       return "INT_LOGIC";
  case INT_SHIFT:       return "INT_SHIFT";
  case INT_MOV:         return "INT_MOV";
  case INT_OTHER:       return "INT_OTHER";

  case FP_ADD:          return "FP_ADD";
  case FP_SUB:          return "FP_SUB";
  case FP_MUL:          return "FP_MUL";
  case FP_DIV:          return "FP_DIV";
  case FP_CMP:          return "FP_CMP";
  case FP_CVT:          return "FP_CVT";
  case FP_SQRT:         return "FP_SQRT";
  case FP_MOV:          return "FP_MOV";
  case FP_OTHER:        return "FP_OTHER";

  case SYS_CALL:        return "SYS_CALL";
  case OTHER:           return "OTHER";
  case INVALID:         return "INVALID";

  default: DIAG_Die("Programming Error");
  }
  return NULL;
}


void
ISA::InsnDesc::dump(std::ostream& o)
{
  o << toString();
}


void
ISA::InsnDesc::ddump()
{
  dump(std::cerr);
}


//***************************************************************************
// binutils helpers
//***************************************************************************


extern "C" {

int
GNUbu_fprintf(void* stream, const char* format, ...)
{
#define BUF_SZ 512
  static char BUF[BUF_SZ];

  va_list args;
  va_start(args, format);
  int n = vsnprintf(BUF, BUF_SZ, format, args);
  va_end(args);

  ostream* os = (ostream*)stream;
  *os << BUF;

  return n;
}


int
GNUbu_fprintf_stub(void* GCC_ATTR_UNUSED stream, 
		   const char* GCC_ATTR_UNUSED format, ...)
{
  return 0;
}


void
GNUbu_print_addr_stub(bfd_vma GCC_ATTR_UNUSED di_vma,
		      struct disassemble_info* GCC_ATTR_UNUSED di)
{
}


int
GNUbu_read_memory(bfd_vma vma, bfd_byte* myaddr, unsigned int len,
		  struct disassemble_info* GCC_ATTR_UNUSED di)
{
  memcpy(myaddr, BFDVMA_TO_PTR(vma, const char*), len);
  return 0; /* success */
}

} // extern "C"

//***************************************************************************
