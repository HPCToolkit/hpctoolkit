// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/trunk/src/lib/isa/x86ISA_xed.cpp $
// $Id: x86ISA_xed.cpp 4175 2013-05-16 20:03:51Z laksono@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/trunk/src/lib/isa/x86ISA_xed.cpp $
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
using std::ostream;

#include <cstdarg> 
#include <cstring> // for 'memcpy'

//*************************** User Include Files ****************************

#include <include/gnu_dis-asm.h>

#include "x86ISAXed.hpp"

#include <lib/support/diagnostics.h>


//*************************** Global Variables ***************************

//*************************** cache decoder ***************************

//*************************** x86ISA ***************************

x86ISAXed::x86ISAXed(bool is_x86_64)
{
}


x86ISAXed::~x86ISAXed()
{
}


ISA::InsnDesc
x86ISAXed::getInsnDesc(MachInsn* mi, ushort GCC_ATTR_UNUSED opIndex,
                    ushort GCC_ATTR_UNUSED s)
{
  ISA::InsnDesc d;

  return d;
}



ushort
x86ISAXed::getInsnSize(MachInsn* mi)
{
  return 0;
}


VMA
x86ISAXed::getInsnTargetVMA(MachInsn* mi, VMA vma, ushort GCC_ATTR_UNUSED opIndex,
                         ushort GCC_ATTR_UNUSED sz)
{
  return 0;
}


//****************************************************************************

