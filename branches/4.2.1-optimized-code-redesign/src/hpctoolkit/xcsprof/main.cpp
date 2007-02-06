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
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
#include <fstream>
#include <new>

#include <string>
using std::string;

#include <sys/stat.h>
#include <sys/types.h>

//*************************** User Include Files ****************************

#include "Args.hpp"
//#include "ProfileReader.h" FIXME
#include "CSProfileUtils.hpp"

#include <lib/binutils/LM.hpp>

//*************************** Forward Declarations ***************************

using std::cerr;

//****************************************************************************

#if 0
#define DEB_ON_LINUX 1  
#endif

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
  Args args(argc, argv);
  
  // ------------------------------------------------------------
  // Read executable
  // ------------------------------------------------------------  

  // Since in the executable is already in the load module
  // We don't need to include the executable in the command
  // line option ! ---FMZ  

#if 0
  Executable* exe = NULL;
  try { 
    exe = new Executable(); 
    if (!exe->Open(args.progFile)) { exit(1); } // Error already printed  
    if (!exe->Read()) { exit(1); }              // Error already printed  
  } catch (std::bad_alloc& x) {
    cerr << "Error: Memory alloc failed while reading binary!\n";
    exit(1);
  } catch (...) {
    cerr << "Error: Exception encountered while reading binary!\n";
    exit(2);
  }   
#endif

#ifdef XCSPROFILE_DEBUG
  exe->Dump(); 
#endif

  // ------------------------------------------------------------
  // Read 'profData', the profiling data file
  // ------------------------------------------------------------
  CSProfile* profData = NULL;
  try {
    //profData = TheProfileReader.ReadProfileFile(args.profileFile /*type*/);
    profData = ReadCSProfileFile_HCSPROFILE(args.profileFile.c_str(),
					    args.progFile.c_str());
    if (!profData) { exit(1); }
  } 
  catch (...) {
    DIAG_EMsg("While reading profile '" << args.profileFile << "'...");
    throw;
  }
  
  // After checking we have samples in the profile, create the database directory
  args.createDatabaseDirectory();
  
  // ------------------------------------------------------------
  // Add source file info and dump
  // ------------------------------------------------------------
  try { 
    
    // for each "used" load module, go through the call stack tree 
    // to find the source line infornmation 


    // for efficiency, we add a flag in CSProfLDmodule "used"
    // add one more pass search the callstack tree, to set 
    // the flag -"alpha"- only, since we get info from binary 
    // profile file not from bfd  

    LdmdSetUsedFlag(profData); 

    int num_lm = profData->GetEpoch()->GetNumLdModule() ;      
    binutils::LM*   lm     = NULL;
    CSProfLDmodule* csp_lm = NULL;
    VMA  begVMA;
    VMA  endVMA = 0; 
    bool lastone;
    
    for (int i = num_lm-1; i >= 0; i--) {
      lastone = (i == num_lm-1);
      
      csp_lm = profData->GetEpoch()->GetLdModule(i); 
      begVMA = csp_lm->GetMapaddr(); // for next csploadmodule  
      
      if (!(csp_lm->GetUsedFlag())) { // ignore unused loadmodule 
	endVMA = begVMA-1;
	continue; 
      }
      
      try {
	lm = new binutils::LM();
	lm->Open(csp_lm->GetName());
	lm->Read();
      }
      catch (...) {
	DIAG_EMsg("While reading '" << csp_lm->GetName() << "'...");
	throw;
      }  
      
      // get the start and end PC from the text sections 
      DIAG_Msg(1, "Load Module: " << csp_lm->GetName());
#if 0
      VMA tmp1,tmp2;
      lm->GetTextStartEndPC(&tmp1,&tmp2);
      lm->SetTextStart(tmp1);
      lm->SetTextEnd(tmp2);
      cout<< "\t LM text started from address : "<< hex <<"0x" << tmp1 << endl;
      cout<< "\t LM text end at the   address : "<< hex <<"0x" << tmp2 << endl; 
      cout<< "\t LM entry point is: "<< hex << "0x" <<  lm->GetTextStart() << endl;
      cout<< "\t data file mapaddress  from   address : "<< hex <<"0x" << begVMA << endl;
      cout<< "\t data file mapaddress  supposed end   : "<< hex <<"0x" << endVMA   <<dec<< endl; 
      // if lm is not an excutable, do relocate  
#endif   
      if ( !(lm->GetType() == binutils::LM::Executable) ) {
	lm->Relocate(begVMA);   
      }
      
      AddSourceFileInfoToCSProfile(profData, lm, begVMA, endVMA, lastone);
      
      // create an extended profile representation
      // normalize call sites 
      //   
      //          main                     main
      //         /    \        =====>        |  
      //        /      \                   foo:<start line foo>
      //       foo:1  foo:2                 /   \
      //                                   /     \
      //                                foo:1   foo:2 
      //  
	
      NormalizeInternalCallSites(profData, lm, begVMA, endVMA, lastone);
	
      endVMA = begVMA-1;
     
      // close current load module and free memory space 
      delete lm;
      
    } /* for each load module */ 

    NormalizeCSProfile(profData);  
     
    // prepare output directory 
    
    string dbSourceDirectory = args.dbDir + "/src";
    if (mkdir(dbSourceDirectory.c_str(),
	      S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
      cerr << "could not create database source code directory " << 
	dbSourceDirectory << endl;
      exit (1);
    }
    
    copySourceFiles(profData, args.searchPaths, dbSourceDirectory);
    
    string experiment_fnm = args.dbDir + "/" + args.OutFilename_XML;
    WriteCSProfileInDatabase(profData, experiment_fnm);
    //WriteCSProfile(profData, std::cout, /* prettyPrint */ true);
  }
  catch (...) {
    DIAG_EMsg("While preparing CSPROFILE...");
    throw;
  }

  delete profData;
  return (0);
}

//****************************************************************************
