// $Id$
// -*-C++-*-
// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
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
//    Assertion.C
//
// Purpose:
//    See Assertion.h for information.
//
// Description:
//    See Assertion.h for information.
//
// History:
//    ??/?? - Originated as some code written by Elana Granston
//            (granston@cs.rice.edu)
//    94/07 - Rewritten, extended and ported to C++ by Mark Anderson
//            (marka@cs.rice.edu)
//    95/08 - Revised with a view to installation in the D System by Mark
//            Anderson (marka) and Kevin Cureton (curetonk)
//
//***************************************************************************

//************************** System Include Files ***************************

#include <fstream>
#include <iostream>

#ifdef NO_STD_CHEADERS
# include <string.h>
# include <stdlib.h>
#else
# include <cstring>
# include <cstdlib>
using namespace std; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************

#include <include/general.h>

#define In_Assertion_C 1

#include "Assertion.h"

//************************** Variable Definitions ***************************

using std::endl;
using std::cerr;

// Global debugging flag initially defaults to local
GlobalAssertMsgsType globalAssertMsgsEnabled = GLOBAL_ASSERT_MSG_LOCAL;

const char* ASSERTION_FAIL_STRING = "Assert Failed";

const int ASSERTION_RETURN_VALUE = 101;

// Used to handle a possible external logfile.
static std::ofstream logfile;

// is external logging on??
static bool externLog = false;

// full assertion trace
int FullAssertionTrace = false; 

//*********************** Static Function Prototypes ************************

static void DumpAssertionInfo(std::ostream & out, 
                              const char * expr,
                              const char * errorDesc, 
                              const char * section, 
                              const char * fileVersion, 
                              const char * fileName,
                              int lineNo); 

static void Quit(AssertionExitType action, int status);


//**************************** External Functions ***************************


////////////////////////////////////////////////////////////////////////////
// Provides convenient external breakpoint for grabbing assertions in debugger
///////////////////////////////////////////////////////////////////////////
void AssertionStopHere(AssertionExitType quitAction)
{
  // Empty code to allow user to select which type to stop on
  switch (quitAction) {
  case ASSERT_ABORT:	
    break;  
  case ASSERT_EXIT:	
    break;
  case ASSERT_NOTIFY:
    break;
  case ASSERT_CONTINUE:      
    break;
  default:
    break;
  }    
}


////////////////////////////////////////////////////////////////////////////
// InitializeErrorChannel()
////////////////////////////////////////////////////////////////////////////

void InitializeErrorChannel(const char * logfilename) 
{
    if ((logfilename != 0) && (strlen(logfilename) >0)) {
        logfile.open(logfilename, std::ios::out /*, filebuf::openprot*/);
	if (logfile.good()) {
	    externLog = true;      
	    return;
	}	
    }

    externLog = false;

    return;

} // Initialize_Error_Channel

////////////////////////////////////////////////////////////////////////////
// NoteFailureThenDie() Handle printing of error message, then goto
// the exit routines
//////////////////////////////////////////////////////////////////////////// 

void NoteFailureThenDie(const char * expr, 
                        const char * errorDesc, 
			const char * fileVersion, 
			const char * fileName, 
                        int lineNo, 
			AssertionExitType quitAction, 
			int status) 
{
  AssertionStopHere(quitAction);

    if ((quitAction == ASSERT_CONTINUE) || 
	((!FullAssertionTrace) && (quitAction == ASSERT_NOTIFY))) return;
    
    DumpAssertionInfo(cerr, expr, errorDesc, (const char*)NULL, 
                      fileVersion, fileName, lineNo);

    if (externLog) {
	DumpAssertionInfo(logfile, expr, errorDesc, 
			  (const char*)NULL, fileVersion, fileName, lineNo);
    }

    Quit(quitAction, status);

    return;

} // NoteFailureAndDie

////////////////////////////////////////////////////////////////////////////
// NoteFailureAndSectionThenDie() Like above, but prints section
//////////////////////////////////////////////////////////////////////////// 

void NoteFailureAndSectionThenDie(const char * expr, 
                                  const char * errorDesc,
 				  const char * section, 
				  const char * fileVersion, 
				  const char * fileName, 
                                  int lineNo, 
				  AssertionExitType quitAction, 
				  int status) 
{
  AssertionStopHere(quitAction);
    if ((quitAction == ASSERT_CONTINUE) || 
	((!FullAssertionTrace) && (quitAction == ASSERT_NOTIFY))) return;
  	
    DumpAssertionInfo(cerr, expr, errorDesc, section,
		      fileVersion, fileName, lineNo);

    if (externLog) {
	DumpAssertionInfo(logfile, expr, errorDesc, section,
			  fileVersion, fileName, lineNo);
    }

    Quit(quitAction, status);

    return;

} // NoteFailureAndDie


//***************************** Static Functions ****************************

////////////////////////////////////////////////////////////////////////////
// DumpAssertionInfo()
//////////////////////////////////////////////////////////////////////////// 

static void DumpAssertionInfo(std::ostream & out, 
                              const char * expr, 
                              const char * errorDesc, 
                              const char * section, 
		              const char * fileVersion,  
                              const char * fileName, 
		              int lineNo)
{
    if (errorDesc != 0 && *errorDesc != 0) {
	out << "ASSERTION FAILED: " << errorDesc << endl;
    }	
    else {
        out << "ASSERTION FAILED:" << endl;
    }

    out << "    File: \"" << fileName << "\", line " << lineNo << endl; 

    out << "    File Version: " << fileVersion << endl;    

    if (section != 0 && *section != 0) {
	out << "    Section: " << section << endl;
    }	

    out << "    Assertion Expression: " << expr << endl;

    out.flush();  			   

    return;
}			           

////////////////////////////////////////////////////////////////////////////
// Quit()
//////////////////////////////////////////////////////////////////////////// 

static void Quit(AssertionExitType action, int status) 
{
    switch (action) 
    {
        case ASSERT_ABORT:	
	    abort();
        break;  

        case ASSERT_EXIT:
	    exit(status);
        break;

        case ASSERT_NOTIFY:
            // Do nothing for this selection
        break;

        case ASSERT_CONTINUE:      
            // Should never reach this point
	    cerr << "Assertion.C: Internal Error in Quit()." 
		 << endl;
        break;

        default:   
            // Should never reach this point
            cerr << "Assertion.C: Internal Error in Quit()." 
                 << endl;
            if (externLog) 
            {
                logfile << "Assertion.C: Internal Error in Quit()." 
			<< endl;
            }
        break;
    }

    return;

}  // end Quit

