// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef Trace_h
#define Trace_h

//************************** System Include Files ***************************

#include <iostream>

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

// Make this global so we can easily trace the whole program
extern int trace;

//***************************************************************************

#ifndef NTRACE
#define IFTRACE        if (trace) std::cerr
#define IFDOTRACE      if (trace)
#else
// the optimizer will take care of this
#define IFTRACE        if (0) std::cerr
#define IFDOTRACE      if (0)
#endif

#define TRACE_METHOD(Class, method)  \
      IFTRACE << (unsigned long) this << "->" << #Class << "::" << #method << " "
#define TRACE_STATIC_METHOD(Class, method)  \
      IFTRACE << "STATIC" << "->" << #Class << "::" << #method << " "
#define SET_TRACE_IMPL(module) void SetTrace ## module( int trc) { trace = trc; }

#endif
