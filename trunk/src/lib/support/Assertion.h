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

/******************************************************************************
 *
 * File:
 *    Assertion.h
 *
 * Purpose:
 *    This package is intended to provide a general assert facility.
 *    This implements assertion macros that test a condition, and if the
 *    condition is false, prints the file, line number and condition
 *    tested to stderr and possibly a logfile.  It then performs a programmer
 *    selected action.  The possible actions are:
 *    
 *       ASSERT_ABORT - exit the program with a core dump.
 *       ASSERT_EXIT -  exit the program.
 *       ASSERT_NOTIFY - print warning and continue. 
 *       ASSERT_CONTINUE - continue without a message (nop assert).
 *
 *    There are two variables that control whether or not assert messages
 *    are printed for ASSERT_NOTIFY assertions.  Messages can be globally 
 *    forced on or off, or put under local control.  The variables to control
 *    this are:
 *
 *       globalAssertMsgsEnabled - a global variable to control assert msgs.
 *                            Can have the values of GLOBAL_ASSERT_MSG_NONE, 
 *                            GLOBAL_ASSERT_MSG_LOCAL, GLOBAL_ASSERT_MSG_ALL.
 *       localAssertMsgsEnabled  - a static local variable for each file
 *                            (included via Assertions.h) to control assert
 *                            msgs.  Can take on the values of 
 *                            LOCAL_ASSERT_MSG_ON and LOCAL_ASSERT_MSG_OFF.
 *
 * Description:
 *    If ASSERTIONS_OFF is undefined, then the asserts will be enabled (this
 *       is the default).
 *
 *    If ASSERTIONS_OFF is defined, then no asserts will occur (except 
 *       for HardAssertion asserts). 
 *
 *    DO NOT PLACE TESTS IN ASSERTS THAT CHANGE PROGRAM STATE! 
 *       Especially do not write asserts that fetch a value that is used 
 *       later.
 *
 *    ASSERT_FILE_VERSION is #define to be the version number of the file.
 *       A good thing to use is the RCS_ID string.
 *
 *    BriefAssertion(expression)
 *       Place BriefAssertion whereever an old style assert was placed. 
 *       This is a one for one replacement. If expression is true, execution
 *       continues, otherwise assert is invoked. The code will abort after
 *       printing a message.
 *   
 *    Assertion(exp, message, quitAction)
 *       If exp false, print message, line number, file name, file version
 *       and then do quitAction.
 *   
 *    LongAssertion(ex, errorDesc, section, quitAction)
 *       If exp false, print errorDesc and section, then do quitAction.
 *    
 *    ProgrammingErrorIfNot(cond)
 *       If condition is false, does Assertion with "Programming error"
 *       and aborts.
 *   
 *    ShouldNotGetHere()
 *       Use in theoretically dead sections of code. Prints "Should
 *       not get here" and aborts.
 *    
 *    NotImplemented()
 *       Like above, except prints "This code has yet to be written"
 *
 *    ThisCodeIsDeprecated()
 *       Like above, except prints "This code is very deprecated, you
 *       should not use"
 *   
 *    HardAssertion(ex, errorDesc, quitAction)
 *       Like Assertion, but always happens regardless of Assertion flag.
 *
 *    InitializeErrorChannel(const char* logFilename)
 *       It is possible to log asserts to an external logfile as well.
 *       Call InitializeErrorChannel with the filename to log into. 
 *       Should only be called once per program, ideally in main().
 *
 * Example:
 *
 *
 *    #define ASSERTION_FILE_VERSION RCS_ID
 *
 *    // other includes go here
 *    #include <lib/support/msgHandlers/Assertion.h>
 *
 *    main()
 *    {
 *       localAssertMsgsEnabled = LOCAL_ASSERT_MSG_ON;
 *        ... 
 *
 *       Assertion(i==j, "Bogus result", ASSERT_NOTIFY);
 *        ...
 *    }
 *
 * History:
 *    ??/?? - Originated as some code written by Elana Granston
 *            (granston@cs.rice.edu)
 *    94/07 - Rewritten, extended and ported to C++ by Mark Anderson
 *            (marka@cs.rice.edu)
 *    95/08 - Revised with a view to installation in the D System by Mark
 *            Anderson (marka) and Kevin Cureton (curetonk)
 *
 *****************************************************************************/

#ifndef Assertion_h
#define Assertion_h

/************************** Enumeration Definitions **************************/

/*****************************************************************************
 *
 *  Enumeration:
 *     AssertionExitType
 *
 *  Enumeration Members:
 *     ASSERT_EXIT     - Exit the program
 *     ASSERT_ABORT    - Abort (dump core and exit) 
 *     ASSERT_NOTIFY   - Print message and continue execution
 *     ASSERT_CONTINUE - Do not print message and continue execution	
 *
 *  Purpose:
 *     To specify the assertion's exit and message behavior
 *
 *  History:
 *     94/11 - marka - initial revision
 *
 ****************************************************************************/
extern int FullAssertionTrace;  
     // governs whether failed ASSERT_NOTIFY type assertions are printed 
     // default is false

enum AssertionExitType 
{ 
   ASSERT_ABORT, 
   ASSERT_EXIT, 
   ASSERT_NOTIFY, 
   ASSERT_CONTINUE
};

/*****************************************************************************
 *
 *  Enumeration:
 *     GlobalAssertMsgsType
 *
 *  Enumeration Members:
 *     GLOBAL_ASSERT_MSG_NONE  - Globally turn off all assert msgs from asserts.
 *     GLOBAL_ASSERT_MSG_LOCAL - Put control of assert msgs under local control.
 *     GLOBAL_ASSERT_MSG_ALL   - Globally turn on all assert msgs from asserts.
 *
 *  Purpose:
 *     Determine the status of global assertion messages.
 *
 *  History:
 *     95/08 - curetonk - initial revision
 *
 ****************************************************************************/
enum GlobalAssertMsgsType
{
   GLOBAL_ASSERT_MSG_NONE  = -1,
   GLOBAL_ASSERT_MSG_LOCAL =  0,
   GLOBAL_ASSERT_MSG_ALL   =  1
};

/*****************************************************************************
 *
 *  Enumeration:
 *     LocalAssertMsgsType
 *
 *  Enumeration Members:
 *     LOCAL_ASSERT_MSG_OFF - Turn off local assert messages from asserts.
 *     LOCAL_ASSERT_MSG_ON  - Turn on local assert messages from asserts.
 *
 *  Purpose:
 *     Determine the status of local assert messages.
 *
 *  History:
 *     95/08 - curetonk - initial revision
 *
 ****************************************************************************/
enum LocalAssertMsgsType
{
   LOCAL_ASSERT_MSG_OFF = 0,
   LOCAL_ASSERT_MSG_ON  = 1
};

/************************* Extern Function Prototypes ***********************/

// Start up logfile to record all messages from assertions
void InitializeErrorChannel(const char * logfile);

// This function should not be called by users directly
void NoteFailureThenDie(const char * ex, 
                        const char * errorDesc,
			const char * fileVersion, 
			const char * fileName, int lineNo, 
			AssertionExitType quitAction, 
			int status);

// This function should not be called by users directly
void NoteFailureAndSectionThenDie(const char * ex, 
                                  const char * errorDesc, 
				  const char * section,  
				  const char * fileVersion, 
				  const char * fileName, int lineNo, 
				  AssertionExitType quitAction, 
				  int status);

// Provides convenient external breakpoint for grabbing assertions in debugger
void AssertionStopHere(AssertionExitType quitAction);


/**************************** Variable Definitions ***************************/

// Default message to print out on failure.
extern const char* ASSERTION_FAIL_STRING;	

// Value returned by program when exit/abort called
extern const int  ASSERTION_RETURN_VALUE;

// Should global assert messages appear
extern GlobalAssertMsgsType globalAssertMsgsEnabled;

#ifndef In_Assertion_C
// ifndef added so this won't be defined and unused in Assertion.C
// Should local assert messages appear
static LocalAssertMsgsType localAssertMsgsEnabled = LOCAL_ASSERT_MSG_ON;
#endif

/***************************** Macro Definitions ****************************/

#ifndef ASSERTION_FILE_VERSION 
#define ASSERTION_FILE_VERSION "No file version info provided"
#endif

# ifndef ASSERTIONS_OFF

#define Assertion(expr, errorDesc, quitAction) \
   { \
      if (((quitAction != ASSERT_NOTIFY) || \
             ((localAssertMsgsEnabled + globalAssertMsgsEnabled) > 0)) && \
          !(expr)) \
      { \
         NoteFailureThenDie(#expr, \
                            errorDesc, \
                            ASSERTION_FILE_VERSION, \
                            __FILE__, \
                            __LINE__, \
                            quitAction, \
                            ASSERTION_RETURN_VALUE); \
      } \
   }


#define LongAssertion(expr, errorDesc, section, quitAction) \
   { \
      if (((quitAction != ASSERT_NOTIFY) || \
             ((localAssertMsgsEnabled + globalAssertMsgsEnabled) > 0)) && \
          !(expr)) \
      { \
         NoteFailureAndSectionThenDie(#expr, \
                                      errorDesc, \
                                      section, \
                                      ASSERTION_FILE_VERSION, \
                                      __FILE__, \
                                      __LINE__, \
                                      quitAction, \
                                      ASSERTION_RETURN_VALUE); \
      } \
   }

# else

#define Assertion(expr, errorDesc, quitAction)
#define LongAssertion(expr, errorDesc, section, quitAction)

#endif  // ASSERTIONS_OFF

// HardAssertion should always be defined
#define HardAssertion(ex, errorDesc, quitAction) \
   { \
      if (!(ex)) \
      { \
         NoteFailureThenDie(#ex, \
                            errorDesc, \
                            ASSERTION_FILE_VERSION, \
                            __FILE__, \
                            __LINE__, \
                            quitAction, \
                            ASSERTION_RETURN_VALUE); \
      } \
   }

#define BriefAssertion(ex) \
   { \
      Assertion(ex, ASSERTION_FAIL_STRING, ASSERT_ABORT); \
   }

#define ProgrammingErrorIfNot(cond) \
   { \
      Assertion((cond), "Programming error!!", ASSERT_ABORT); \
   }

#define ShouldNotGetHere() \
   { \
      Assertion(false, "Should not get here!!", ASSERT_ABORT); \
   }

#define NotImplemented() \
   { \
      Assertion(false, "This code is not implemented!!", ASSERT_ABORT); \
   }

#define ThisCodeIsDeprecated() \
   { \
      Assertion(false, "This code is deprecated!!", ASSERT_NOTIFY); \
   }

#define VerifyMemoryAllocation(ptr) \
   { \
      Assertion(ptr != NULL, "Memory allocation failure!", ASSERT_ABORT); \
   }

#endif
