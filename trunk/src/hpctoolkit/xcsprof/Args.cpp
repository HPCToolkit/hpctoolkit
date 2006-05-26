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

//************************* System Include Files ****************************

#include <iostream>

#include <unistd.h> // for 'getopt'
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/errno.h>

//*************************** User Include Files ****************************

#include "Args.hpp"
#include <lib/support/String.hpp>
#include <lib/support/StringUtilities.hpp>
#include <lib/support/Trace.hpp>
#include "CSProfileUtils.hpp"

//*************************** Forward Declarations **************************

using std::cerr;
using std::endl;

//***************************************************************************

const char* hpctoolsVerInfo=
#include <include/HPCToolkitVersionInfo.h>

void Args::Version()
{
  cerr << cmd << ": " << hpctoolsVerInfo << endl;
}

void Args::Usage()
{
  cerr
    << "Usage: " << endl
    << "  " << cmd << " [-h|-H|-help|--help] [-V|--V|-version|--version] <binary> <profile> [-I src1 ... srcn] -db database \n"
    << endl;
  cerr
    << "Converts one csprof output file into a CSPROFILE database.\n"
    << "The results are created in the XML file database/xcsprof.xml.\n"
    << "The available source files are copied in the directory database/src.\n"
    << "Options:\n"
    << " -h|-H|-help|--help          : print this help\n"    
    << " -V|--V|-version|--version   : print version information\n"    
    << " -I src1 ... srcn : search paths for source files\n"    
    << endl;
} 

Args::Args(int argc, char* const* argv)
{
  cmd = argv[0]; 
  xDEBUG (DEB_PROCESS_ARGUMENTS, 
	  cerr << "start processing arguments\n";);

  bool printVersion = false;
  bool printHelp = false;
  //other options: prettyPrintOutput = false;

  if (argc == 1) {
    Usage();
    exit(1);
  }

  int argIndex=1;
  
  if ( !strcmp( argv[argIndex], "-h") || !strcmp( argv[argIndex], "-help") ||
       !strcmp( argv[argIndex], "-H") || !strcmp( argv[argIndex], "--help")) {
    Usage();
    exit(1);
  }

  if ( !strcmp( argv[argIndex], "-V") || !strcmp( argv[argIndex], "--V") ||
       !strcmp( argv[argIndex], "-version") || !strcmp( argv[argIndex], "--version")) {
    Version();
    exit(1);
  }

  xDEBUG (DEB_PROCESS_ARGUMENTS, 
	  cerr << "argc=" << argc << endl;);

  if (argc < 3) {
    Usage ();
    exit(1);
  } 

  progFile = argv[argIndex]; 
  profFile = argv[argIndex+1]; 
  argIndex = 3;

  char cwdName[MAX_PATH_SIZE +1];
  getcwd(cwdName, MAX_PATH_SIZE);
  String crtDir=cwdName; 
  if ( (argc>argIndex) && !strcmp(argv[argIndex], "-I")) {
    xDEBUG (DEB_PROCESS_ARGUMENTS, 
	    cerr << "adding search paths" << endl;);
    argIndex ++;
    while ( (argIndex < argc) &&  strcmp(argv[argIndex], "-db")) {
      xDEBUG (DEB_PROCESS_ARGUMENTS, 
	      cerr << "adding search path  " << 
	      argv[argIndex] << endl;);
      String searchPath = argv[argIndex];
      if (chdir (argv[argIndex]) == 0) {
	char searchPathChr[MAX_PATH_SIZE +1];
	getcwd(searchPathChr, MAX_PATH_SIZE);
	String normSearchPath=searchPathChr; 
	//String normSearchPath = normalizeFilePath(searchPath);
	searchPaths.push_back(normSearchPath);
      }
      chdir (cwdName);
      argIndex++;
    }
  }

  xDEBUG (DEB_PROCESS_ARGUMENTS, 
	  cerr << "argc=" << argc << 
	  " argIndex=" << argIndex << endl;);
  if ( (argc != (argIndex+2)) ||
       (strcmp(argv[argIndex], "-db"))) {
    databaseDirectory = crtDir+"/xcsprof-db";
  } else {
    databaseDirectory = argv[argIndex+1];
    databaseDirectory = normalizeFilePath (databaseDirectory);
  }

  /*

  bool uniqueDatabaseDirectoryCreated ;

  if (mkdir(databaseDirectory, 
	    S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
    if (errno == EEXIST) {
      cerr << databaseDirectory << " already exists\n";
      // attempt to create databaseDirectory+pid;
      char myPidChr[20];
      pid_t myPid = getpid();
      itoa (myPid, myPidChr);
      String databaseDirectoryPid = databaseDirectory + myPidChr;
      cerr << "attempting to create alternate database directory "
	   << databaseDirectoryPid << std::endl;
      if (mkdir(databaseDirectoryPid, 
		S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
	cerr << "could not create alternate database directory " << 
	  databaseDirectoryPid << std::endl;
	exit(1);
      } else {
	cerr << "created unique database directory " << databaseDirectoryPid << std::endl;
	databaseDirectory = databaseDirectoryPid;
      }

    } else {
      cerr << "could not create database directory " << 
	databaseDirectory << endl;
      exit (1);
    }
  }
  */

  cerr << "executable: " << progFile << " csprof trace: " << profFile << std::endl;
  if (searchPaths.size() > 0) {
    cerr << "search paths: " << std::endl; 
    for (int i=0; i<searchPaths.size(); i++ ) {
	cerr << searchPaths[i] << std::endl;
      }
  }
  cerr << "Database directory: " << databaseDirectory << std::endl;
}


void 
Args::createDatabaseDirectory() {
  bool uniqueDatabaseDirectoryCreated ;

  if (mkdir(databaseDirectory, 
	    S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
    if (errno == EEXIST) {
      cerr << databaseDirectory << " already exists\n";
      // attempt to create databaseDirectory+pid;
      char myPidChr[20];
      pid_t myPid = getpid();
      itoa (myPid, myPidChr);
      String databaseDirectoryPid = databaseDirectory + myPidChr;
      cerr << "attempting to create alternate database directory "
	   << databaseDirectoryPid << std::endl;
      if (mkdir(databaseDirectoryPid, 
		S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
	cerr << "could not create alternate database directory " << 
	  databaseDirectoryPid << std::endl;
	exit(1);
      } else {
	cerr << "created unique database directory " << databaseDirectoryPid << std::endl;
	databaseDirectory = databaseDirectoryPid;
      }

    } else {
      cerr << "could not create database directory " << 
	databaseDirectory << endl;
      exit (1);
    }
  }
}
