// -*-Mode: C++;-*-
// $Id$

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
//   $Source$
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

#ifndef CmdLineParser_H 
#define CmdLineParser_H

//************************* System Include Files ****************************

#include <iostream>
#include <map>
#include <vector>
#include <string>

//*************************** User Include Files ****************************

#include <inttypes.h> /* commonly available, unlike <stdint.h> */

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
//   [arguments] ::= [optional_arg] | [regular_arg]
//   
//   [optional_arg] ::= -f [arg] | -f[arg]
//                    | --foo [arg] | --foo=arg
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
  
  struct OptArgDesc {
    
    bool operator==(const OptArgDesc& x) const { 
      return (swShort == x.swShort && swLong == x.swLong 
	      && kind == x.kind && dupKind == x.dupKind
	      && dupArgSep == x.dupArgSep);
    }
    bool operator!=(const OptArgDesc& x) const { return !(*this == x); }
    
    // Data
    char swShort;       // 0 if n/a
    const char* swLong; // NULL if n/a
    OptKind kind;
    DupOptKind dupKind;
    const char* dupArgSep; // separator for 'DUPARG_CONCAT'
  };
  
  // The NULL terminator (two versions).  The use of the first version
  // is preferable, but some older compilers won't support it.
  static OptArgDesc OptArgDesc_NULL;
# define CmdLineParser_OptArgDesc_NULL_MACRO \
    { 0, NULL, CmdLineParser::ARG_NULL, CmdLineParser::DUPOPT_NULL, NULL }
  
  
  // ---------------------------------------------------------
  // Exception thrown when errors are encountered
  // ---------------------------------------------------------
  
  class Exception {
  public:
    Exception(const char* m) : msg(m) { }
    Exception(std::string m) : msg(m) { }
    virtual ~Exception () { }

    virtual const std::string& GetMessage() const { return msg; }
    virtual void Report(std::ostream& os) const { 
      os << "CmdLineParser::Exception: " << msg << std::endl; 
    }    
    virtual void Report() const { Report(std::cerr); }

  protected: 
    std::string msg;
  };

  class ParseError : public Exception {
  public:
    ParseError(const char* m) : Exception(m) { }
    ParseError(std::string m) : Exception(m) { }
    virtual ~ParseError () { }
  };

  class InternalError : public Exception {
  public:
    InternalError(const char* m) : Exception(m) { }
    InternalError(std::string m) : Exception(m) { }
    virtual ~InternalError () { }
  private:
    void Ctor() {
      msg = "CmdLineParser internal error (Don't abuse me!): " + msg;
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
  Parse(const OptArgDesc* optArgDescs, 
	int argc, const char* const argv[]);
  
  // -------------------------------------------------------
  // Parsed Data: Command
  // -------------------------------------------------------

  // GetCmd: The command (will be valid even after a parse error)
  const std::string& GetCmd() const;
  
  // -------------------------------------------------------
  // Parsed Data: Optional arguments
  // -------------------------------------------------------
  
  // IsOpt: (IsOption) Given a short or long switch, returns whether
  // the switch has been seen.
  bool IsOpt(const char swShort) const;
  bool IsOpt(const char* swLong) const;
  bool IsOpt(const std::string& sw) const;

  // IsOptArg: (IsOptionArgument) Given a short or long switch,
  // returns whether an argument is associated with it.  Designed for
  // switches that optionally take arguments.
  bool IsOptArg(const char swShort) const;
  bool IsOptArg(const char* swLong) const;
  bool IsOptArg(const std::string& sw) const;  
  
  // GetOptArg: (GetOptionArgument) Given a short or long switch, get
  // the argument associated with it.  Assumes user has verified that
  // an argument *exists*.
  const std::string& GetOptArg(const char swShort) const;
  const std::string& GetOptArg(const char* swLong) const;
  const std::string& GetOptArg(const std::string& sw) const;

  // -------------------------------------------------------
  // Parsed Data: Arguments
  // -------------------------------------------------------
  unsigned int GetNumArgs() const;
  const std::string& GetArg(unsigned int i) const;

  // -------------------------------------------------------
  // Convert strings into other formats
  // -------------------------------------------------------
  // The input should be non-empty
  static long     ToLong(const std::string& str);
  static uint64_t ToUInt64(const std::string& str);
  static double   ToDbl(const std::string& str);
  
  // -------------------------------------------------------
  // Misc
  // -------------------------------------------------------
  void Dump(std::ostream& os = std::cerr) const;
  void DDump() const;
  
private:
  // Should not be used 
  CmdLineParser(const CmdLineParser& p) { }
  CmdLineParser& operator=(const CmdLineParser& x) { return *this; }
  
  typedef std::map<std::string, std::string*> SwitchToArgMap;
  typedef std::vector<std::string> ArgVec;

  // Switch descriptor (Because of limited use, we allow this to be
  // returned as an object)
  class SwDesc {
  public:
    SwDesc() : isLong(false) { }
    SwDesc(const char* sw_, bool isLong_, const char* arg_) 
      : sw(sw_), isLong(isLong_), arg(arg_) { }
    SwDesc(const std::string& sw_, bool isLong_, const std::string& arg_) 
      : sw(sw_), isLong(isLong_), arg(arg_) { }
    ~SwDesc() { }
    // use default copy constructor if necessary
    
    std::string sw;  // switch text without dashes
    bool isLong;     // long style
    std::string arg; // any argument
  };

private:
  void Ctor();
  void Reset();
  void CheckForErrors(const OptArgDesc* optArgDescs);

  // Parsing helpers
  SwDesc
  MakeSwitchDesc(const char* str);
  
  const OptArgDesc*
  FindOptDesc(const OptArgDesc* optArgDescs, const SwDesc& swdesc,
	      bool errOnMultipleMatches = true);
  
  void
  AddOption(const OptArgDesc& odesc, const SwDesc& swdesc);
  
  void
  AddOption(const OptArgDesc& odesc, 
	    const std::string& sw, const std::string& arg);
  
private:
  
  std::string command;           // comand name
  SwitchToArgMap switchToArgMap; // optional arguments
  ArgVec arguments;              // regular arguments
};

//****************************************************************************

#endif

