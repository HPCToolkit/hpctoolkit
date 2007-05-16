// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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
// DumpMsgHandler.C: 
// 
// Author:  John Mellor-Crummey                               October 1993
//
//                                                                          
//***************************************************************************

//************************** System Include Files ***************************

#ifdef NO_STD_CHEADERS
# include <stdio.h>
#else
# include <cstdio>
using namespace std; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************

#include <include/general.h>

#include "DumpMsgHandler.hpp"

//*************************** Forward Declarations **************************

#define SPACES_PER_LEVEL 2

//***************************************************************************

/* Print error message and die. */
void die_with_message(char* format, ...)
{
  va_list  arg_list;		/* the argument list as per varargs	*/

  va_start(arg_list, format);
    {
       fprintf(stderr, "Abnormal termination:  ");
       vfprintf(stderr, format, arg_list);
       fprintf(stderr, "\n");
    }
  va_end(arg_list);

  abort();

  return;
}

//***************************************************************************

//*******************************************************************
// declarations 
//*******************************************************************


DumpMsgHandler dumpHandler;

unsigned int DumpMsgHandler::nestingDepth = 0;
unsigned DumpMsgHandler::indentEnabled = 1;


//**********************
// forward declarations
//**********************


//***************************************************************************
// class DumpMsgHandler interface operations
//***************************************************************************



void DumpMsgHandler::BeginScope()
{
  nestingDepth++;
}


void DumpMsgHandler::EndScope()
{
  nestingDepth--;
}


void DumpMsgHandler::AddLevel(int nlevels)
{
  nestingDepth += nlevels;
}


void DumpMsgHandler::SubLevel(int nlevels)
{
  nestingDepth -= nlevels;
}


// Dump a message to stderr
//
int DumpMsgHandler::Dump(const char *format, ...)
{
  int retval = 0;
  va_list args;
  va_start(args, format);

  if (indentEnabled) {
    retval = fprintf(stderr, "%*s", SPACES_PER_LEVEL * nestingDepth, "");
    if (retval < 0) return retval;
  }
  retval = vfprintf(stderr, format, args);

  va_end(args);
  return retval;
}

// Dump a message to a file
//
int DumpMsgHandler::DumpToFile(FILE *fp, const char *format, ...)
{
  int retval = 0;

  va_list args;
  va_start(args, format);

  if (indentEnabled) {
    retval = fprintf(fp, "%*s", SPACES_PER_LEVEL * nestingDepth, "");
    if (retval < 0) return retval;
  }
  retval = vfprintf(fp, format, args);

  va_end(args);
  return retval;
}


unsigned int DumpMsgHandler::Indentation()
{
  return SPACES_PER_LEVEL * nestingDepth;
}


unsigned DumpMsgHandler::IndentationControl(unsigned onoff)
{
  unsigned rval = indentEnabled;
  indentEnabled = onoff;
  return rval;
}
