// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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
// DumpMsgHandler.h: 
// 
// Author:  John Mellor-Crummey                               July 1994
//
//                                                                          
//***************************************************************************

#ifndef DumpMsgHandler_h
#define DumpMsgHandler_h

//************************** System Include Files ***************************

#include <cstdarg>
#include <cstdio> // for 'FILE'

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

//***************************************************************************

/* die_with_message:                                                    */
/* (eraxxon: Moved here from general.h)                                 */
/* Takes a variable number of arguments.  The first argument must always*/
/* be the format parameter for the output string.  Following arguments	*/
/* are based upon the sprintf-style argument list for the format string.*/
/* The routine prints a message based upon the input and terminates the	*/
/* environment abnormally.						*/
extern void die_with_message (char *format, ...);

//***************************************************************************
// class DumpMsgHandler
//***************************************************************************

class DumpMsgHandler {
public: 
  // 
  // Begin scope (increases indentation level one notch)
  // 
  void BeginScope();

  // 
  // End scope (decreases indentation level one notch)
  //
  void EndScope();

  // 
  // Dump a message to stderr
  //
  int Dump(const char *format, ...); 

  // 
  // Dump a message to a file
  //
  int DumpToFile(FILE *fp, const char *format, ...); 

  // 
  // Returns current indentation level in spaces
  //
  unsigned int Indentation(); // returns current indentation level in spaces
  
  // 
  // Enables/disables indentation. Nonzero value will enable
  // indentation, zero value will disabled. Returns previous
  // flag value.
  //
  unsigned IndentationControl(unsigned onoff);

  //
  // Like BeginScope/EndScope, but adds/subtracts specified number
  // indentation levels (instead of just 1). 
  //
  void AddLevel(int nlevels);
  void SubLevel(int nlevels);

private:
  static unsigned int nestingDepth;
  static unsigned indentEnabled;
};


//***************************************************************************
// external declarations
//***************************************************************************
extern DumpMsgHandler dumpHandler;

#endif
