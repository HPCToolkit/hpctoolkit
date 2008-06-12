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

//************************ System Include Files ******************************

#include <iostream>
#include <fstream>

#include <string>
using std::string;

#include <exception>

#ifdef NO_STD_CHEADERS
# include <string.h>
#else
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

//*********************** Xerces Include Files *******************************

//************************* User Include Files *******************************

#include "Args.hpp"
#include "ConfigParser.hpp"

#include <lib/analysis/Flat_SrcCorrelation.hpp>

#include <lib/prof-juicy-x/XercesUtil.hpp>
#include <lib/prof-juicy-x/XercesErrorHandler.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/NaN.h>
#include <lib/support/IOUtil.hpp>

//************************ Forward Declarations ******************************

static void
readConfFile(Args& args, Prof::Metric::Mgr& metricMgr);

//****************************************************************************

int realmain(int argc, char* const* argv);

int 
main(int argc, char* const* argv) 
{
  int ret;

  try {
    ret = realmain(argc, argv);
  }
  catch (const Diagnostics::Exception& x) {
    DIAG_EMsg(x.message());
    exit(1);
  } 
  catch (const std::bad_alloc& x) {
    DIAG_EMsg("[std::bad_alloc] " << x.what());
    exit(1);
  }
  catch (const std::exception& x) {
    DIAG_EMsg("[std::exception] " << x.what());
    exit(1);
  }
  catch (...) {
    DIAG_EMsg("Unknown exception encountered!");
    exit(2);
  }

  return ret;
}


int 
realmain(int argc, char* const* argv) 
{
  Args args(argc, argv);  // exits if error on command line

  NaN_init();

  //-------------------------------------------------------
  // Create metric descriptors (and for conf file, rest of the args)
  //-------------------------------------------------------
  Prof::Metric::Mgr metricMgr;

  if (args.configurationFileMode) {
    readConfFile(args, metricMgr); // exits on failure
  }
  else {
    metricMgr.makeRawMetrics(args.profileFiles);
  }
  
  //-------------------------------------------------------
  // Correlate metrics with program structure and Generate output
  //-------------------------------------------------------
  Prof::Struct::Tree structure("", new Prof::Struct::Pgm(""));

  Analysis::Flat::Driver driver(args, metricMgr, structure);
  int ret = driver.run();

  if (!args.configurationFileMode) {
    string configfnm = args.db_dir + "/config.xml";
    std::ostream* os = IOUtil::OpenOStream(configfnm.c_str());
    driver.write_config(*os);
    IOUtil::CloseStream(os);
  }
  
  return ret;
} 


//****************************************************************************
//
//****************************************************************************

#define NUM_PREFIX_LINES 2

static string 
buildConfFile(const string& hpcHome, const string& confFile);

static void 
appendContents(std::ofstream &dest, const char *srcFile);

static void
readConfFile(Args& args, Prof::Metric::Mgr& metricMgr)
{
  InitXerces(); // exits iff failure 

  const string& cfgFile = args.configurationFile;
  DIAG_Msg(2, "Initializing from: " << cfgFile);
  
  string tmpFile = buildConfFile(args.hpcHome, cfgFile);
  
  try {
    XercesErrorHandler errHndlr(cfgFile, tmpFile, NUM_PREFIX_LINES, true);
    ConfigParser parser(tmpFile, errHndlr);
    parser.parse(args, metricMgr);
  }
  catch (const SAXParseException& x) {
    unlink(tmpFile.c_str());
    //DIAG_EMsg(XMLString::transcode(x.getMessage()));
    exit(1);
  }
  catch (const ConfigParserException& x) {
    unlink(tmpFile.c_str());
    DIAG_EMsg(x.message());
    exit(1);
  }
  catch (...) {
    unlink(tmpFile.c_str());
    DIAG_EMsg("While processing '" << cfgFile << "'...");
    throw;
  };

  unlink(tmpFile.c_str());

  FiniXerces();
}


static string
buildConfFile(const string& hpcHome, const string& confFile) 
{
  string tmpFile = FileUtil::tmpname(); 
  string hpcloc = hpcHome;
  if (hpcloc[hpcloc.length()-1] != '/') {
    hpcloc += "/";
  }
  std::ofstream tmp(tmpFile.c_str(), std::ios_base::out);

  if (tmp.fail()) {
    DIAG_Throw("Unable to write temporary file: " << tmpFile);
  }
  
  // the number of lines added below must equal NUM_PREFIX_LINES
  tmp << "<?xml version=\"1.0\"?>" << std::endl 
      << "<!DOCTYPE HPCVIEW SYSTEM \"" << hpcloc // has trailing '/'
      << "share/hpctoolkit/dtd/HPCView.dtd\">" << std::endl;

  //std::cout << "TMP DTD file: '" << tmpFile << "'" << std::std::endl;
  //std::cout << "  " << hpcloc << std::endl;

  appendContents(tmp, confFile.c_str());
  tmp.close();
  return tmpFile; 
}


static void 
appendContents(std::ofstream &dest, const char *srcFile)
{
#define MAX_IO_SIZE (64 * 1024)

  std::ifstream src(srcFile);
  if (src.fail()) {
    DIAG_Throw("Unable to read file: " << srcFile);
  }

  char buf[MAX_IO_SIZE]; 
  for(; !src.eof(); ) {
    src.read(buf, MAX_IO_SIZE);

    ssize_t nRead = src.gcount();
    if (nRead == 0) break;
    dest.write(buf, nRead); 
    if (dest.fail()) {
      DIAG_Throw("appendContents: failed!");
    }
  } 
  src.close();
}

