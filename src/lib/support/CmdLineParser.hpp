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
//   $HeadURL$
//
//   Nathan Tallent
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef support_CmdLineParser_hpp 
#define support_CmdLineParser_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <map>
#include <vector>
#include <string>

#include <stdint.h>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>

#include "diagnostics.h"

//*************************** Forward Declarations ***************************

//****************************************************************************

//****************************************************************************
// CmdLineParser
//****************************************************************************

// CmdLineParser: Parses arguments on the command line, argc and argv.
// Provides easy access to both optional arguments (with short or long
// specifiers) and regular arguments.  Provides functionality similar
// to getopt() and GNU's getopt_long(), but in an easier to use -- and
// in the case of getopt_long(), more portable -- package.  In
// addition, the package provides configurable handling of duplicate
// options/arguments and routines to convert string arguments into
// numerical types.
//
// A user creates a NULL-terminated array of option descriptors
// (OptArgDesc) indicating switch names and arguments, if any.  He
// then instantiates a CmdLineParser and parses argc/argv.  Errors are
// delivered with the exception Exception.  The parser object provides
// access to all optional and regular arguments using the interface
// routines belows.
// 
// More details:
// The command line generally has the form (but note qualifications below):
//   <command> [arguments]
//   
//   <argument>      ::= | <switch> | <switch_arg> | <switch_optarg> | <word>
//   
//   <switch>        ::= -f | --foo  (<short_switch> | <long_switch>)
//
//   <switch_arg>    ::= -f arg   | -farg   | --foo arg   | --foo=arg
//
//   <switch_optarg> ::= -f [arg] | -f[arg] | --foo [arg] | --foo[=arg]
//
// An element of argv that starts with '-' or '--' (and is not exactly
// '--') signifies an option.  The option switch is the text without
// the initial dashes.  As the above shows, we support a variety of
// option styles.
//
// Features: 
//   - The '--' token forces the end of optional argument scanning and
//     iterprets everything following as a list of regular arguments.
//     This is useful for non-option arguments that begin with dashes.
//   - Unlike getopt() we do not support the deprecated use of the single
//     '-'; this is an error.  
//   - Long argument switches may be abbreviated if the abbreviation 
//     is unique. 
//   - Configurable handling of duplicate options and arguments.
//
// Limitations:
//   - Unlike getopt(), we do not currently support short switch grouping,
//     e.g. using -abc instead of -a -b -c.  [FIXME: we can assume that 
//     only options without arguments are allowed to be grouped.]
//
// Warnings:
//   *** NOTE: the following should now be resolved with IsOptArg_fn ***
//   
//   - Switches that take optional arguments can be confusing.  For
//     example, assume a command 'foo' takes a filename and on option,
//     --debug, which itself takes an optional debug level.  The
//     following commands pose no difficulties:
//       foo myfile
//       foo --debug=3 myfile
//       foo --debug 3 myfile
//       foo myfile --debug
//     However, in the following
//       foo --debug myfile
//     'myfile' is erroneously assumed to be the optional argument to
//     --debug.  While the '--' token solves this, it remains awkward.
//
class CmdLineParser {
public:

  // ---------------------------------------------------------
  // Structure used to describe command line options
  // ---------------------------------------------------------
  
  // Describes if an option switch takes an argument
  enum OptKind { 
    ARG_NULL = 0,
    ARG_NONE, // switch does not take argument
    ARG_REQ,  // switch must take an argument
    ARG_OPT   // switch takes an (optional!) argument
  };
  
  // Describes how to handle duplicate options and option arguments
  enum DupOptKind {
    DUPOPT_NULL = 0,
    DUPOPT_ERR,  // throw an exception for duplicate option or argument
    DUPOPT_CLOB, // clobber any previous argument
    DUPOPT_CAT   // concat all available arguments using 'dupArgSep'
  };
  

  // A callback for disambiguating between a switch's potential 
  // optional argument and an actual argument; cf. Warnings above.
  // INVARIANT: 'str' is non-NULL
  typedef bool (*IsOptArg_fn_t)(const char* str);

  struct OptArgDesc {
    
    bool operator==(const OptArgDesc& x) const
    { 
      return (swShort == x.swShort && swLong == x.swLong
	      && kind == x.kind && dupKind == x.dupKind
	      && dupArgSep == x.dupArgSep);
    }

    bool
    operator!=(const OptArgDesc& x) const
    { return !(*this == x); }
    
    // Data
    char swShort;       // 0 if n/a
    const char* swLong; // NULL if n/a
    OptKind kind;
    DupOptKind dupKind;
    const char* dupArgSep; // separator for 'DUPARG_CONCAT'
    IsOptArg_fn_t isOptArgFn; // NULL for default handler
  };


  // The NULL terminator (two versions).  The use of the first version
  // is preferable, but some older compilers won't support it.
  static OptArgDesc OptArgDesc_NULL;
# define CmdLineParser_OptArgDesc_NULL_MACRO \
    { 0, NULL, CmdLineParser::ARG_NULL, CmdLineParser::DUPOPT_NULL, NULL }
  
  
  // ---------------------------------------------------------
  // Exception thrown when errors are encountered
  // ---------------------------------------------------------
  
  class Exception : public Diagnostics::Exception {
  public:
    Exception(const char* x,
	      const char* filenm = NULL, unsigned int lineno = 0)
      : Diagnostics::Exception(x, filenm, lineno) 
      { }

    Exception(std::string x,
	      const char* filenm = NULL, unsigned int lineno = 0) 
      : Diagnostics::Exception(x, filenm, lineno) 
      { }

    ~Exception() { }
  };

  class ParseError : public Exception {
  public:
    ParseError(const char* x,
	       const char* filenm = NULL, unsigned int lineno = 0) 
      : Exception(x, filenm, lineno) 
      { }

    ParseError(std::string x,
	       const char* filenm = NULL, unsigned int lineno = 0) 
      : Exception(x, filenm, lineno) 
      { }
    
    virtual std::string message() const { 
      return "[CmdLineParser::ParseError]: " + what();
    }
  };

  class InternalError : public Exception {
  public:
    InternalError(const char* x,
		  const char* filenm = NULL, unsigned int lineno = 0) 
      : Exception(x, filenm, lineno) 
      { }

    InternalError(std::string x, 
		  const char* filenm = NULL, unsigned int lineno = 0)
      : Exception(x, filenm, lineno) 
      { }

    virtual std::string message() const { 
      return "[CmdLineParser::InternalError]: " + what();
    }
  };
  
  // ---------------------------------------------------------

public:
  // ---------------------------------------------------------
  // Constructor/Destructor
  // ---------------------------------------------------------
  CmdLineParser(); 
  CmdLineParser(const OptArgDesc* optArgDescs, 
		int argc, const char* const argv[]); 
  ~CmdLineParser();

  // -------------------------------------------------------
  // Parsing
  // -------------------------------------------------------
  
  // Parse: Given a NULL terminated array of OptArgDesc describing
  // command line arguments, parses the argv/argc into switches,
  // optional and required arguments.
  void
  parse(const OptArgDesc* optArgDescs,
	int argc, const char* const argv[]);
  
  // -------------------------------------------------------
  // Parsed Data: Command
  // -------------------------------------------------------

  // GetCmd: The command (will be valid even after a parse error)
  const std::string&
  getCmd() const;
  
  // -------------------------------------------------------
  // Parsed Data: Optional arguments
  // -------------------------------------------------------
  
  // isOpt: (isOption) Given a short or long switch, returns whether
  // the switch has been seen.
  bool
  isOpt(const char swShort) const;
  
  bool
  isOpt(const char* swLong) const;

  bool
  isOpt(const std::string& sw) const;

  // isOptArg: (isOptionArgument) Given a short or long switch,
  // returns whether an argument is associated with it.  Designed for
  // switches that optionally take arguments.
  bool
  isOptArg(const char swShort) const;

  bool
  isOptArg(const char* swLong) const;

  bool
  isOptArg(const std::string& sw) const;
  
  // getOptArg: (GetOptionArgument) Given a short or long switch, get
  // the argument associated with it.  Assumes user has verified that
  // an argument *exists*.
  const std::string&
  getOptArg(const char swShort) const;
  
  const std::string&
  getOptArg(const char* swLong) const;
  
  const std::string&
  getOptArg(const std::string& sw) const;

  // -------------------------------------------------------
  // Parsed Data: Arguments
  // -------------------------------------------------------
  unsigned int
  getNumArgs() const;
  
  const std::string&
  getArg(unsigned int i) const;

  // -------------------------------------------------------
  // Convert strings into other formats
  // -------------------------------------------------------
  // The input should be non-empty
  static long
  toLong(const std::string& str);
  
  static uint64_t
  toUInt64(const std::string& str);
  
  static double
  toDbl(const std::string& str);

  // ---------------------------------------------------------
  // Disambiguate optional arguments to a switch
  // ---------------------------------------------------------
  static bool
  isOptArg_long(const char* option);

  // ---------------------------------------------------------
  // Convenience routines for interpreting the value of an option  
  // ---------------------------------------------------------
  static bool
  parseArg_bool(const std::string& value, const char* errTag);

  // -------------------------------------------------------
  // Misc
  // -------------------------------------------------------
  void
  dump(std::ostream& os = std::cerr) const;
  
  void
  ddump() const;
  
private:
  // Should not be used 
  CmdLineParser(const CmdLineParser& GCC_ATTR_UNUSED x)
  { }
  
  CmdLineParser&
  operator=(const CmdLineParser& GCC_ATTR_UNUSED x)
  { return *this; }
  
  typedef std::map<std::string, std::string*> SwitchToArgMap;
  typedef std::vector<std::string> ArgVec;

  // Switch descriptor (Because of limited use, we allow this to be
  // returned as an object)
  class SwDesc {
  public:
    SwDesc() 
      : isLong(false)
    { }

    SwDesc(const char* sw_, bool isLong_, const char* arg_) 
      : sw(sw_), isLong(isLong_), arg(arg_)
    { }
    
    SwDesc(const std::string& sw_, bool isLong_, const std::string& arg_) 
      : sw(sw_), isLong(isLong_), arg(arg_)
    { }
    
    ~SwDesc() { }
    
    // use default copy constructor if necessary

    std::string sw;  // switch text without dashes
    bool isLong;     // long style
    std::string arg; // any argument
  };

private:
  void
  Ctor();
  
  void
  reset();

  void
  checkForErrors(const OptArgDesc* optArgDescs);

  const OptArgDesc*
  createSortedCopy(const OptArgDesc* optArgDescs);

  // Parsing helpers
  SwDesc
  makeSwitchDesc(const char* str);
  
  const OptArgDesc*
  findOptDesc(const OptArgDesc* optArgDescs, const SwDesc& swdesc,
	      bool errOnMultipleMatches = true);
  
  void
  addOption(const OptArgDesc& odesc, const SwDesc& swdesc);
  
  void
  addOption(const OptArgDesc& odesc,
	    const std::string& sw, const std::string& arg);
  
private:
  std::string command;           // comand name
  SwitchToArgMap switchToArgMap; // optional arguments
  ArgVec arguments;              // regular arguments
};

//****************************************************************************

#endif // support_CmdLineParser_hpp

