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

    int numberofldmd =profData->GetEpoch()->GetNumLdModule() ;      
    binutils::LM*      ldmd   = NULL; 
    CSProfLDmodule * csploadmd= NULL; 
    VMA             startaddr;
    VMA             endaddr = 0; 
    bool             lastone ;
    
    for (int i=numberofldmd-1; i>=0; i--) {   
      if (i==numberofldmd-1)
	lastone = true;
      else 
	lastone = false; 
      
      csploadmd = profData->GetEpoch()->GetLdModule(i); 
      
      startaddr = csploadmd->GetMapaddr(); // for next csploadmodule  
      
      if (!(csploadmd->GetUsedFlag())){     //ignore unused loadmodule 
	endaddr = startaddr-1;
	continue; 
      }
      
      try {
	ldmd = new binutils::LM();
	if (!ldmd->Open(csploadmd->GetName())) //Error message already printed
	  continue;
	if (!ldmd->Read())   //Error message already printed
	  continue;
      }
      catch (...) {
	DIAG_EMsg("While reading load module '" << csploadmd->GetName() << "'...");
	throw;
      }  
      
      // get the start and end PC from the text sections 
      cout << "*****Current load module is : " << csploadmd->GetName()<<"*****"<< endl; 
#if 0
      VMA tmp1,tmp2;
      ldmd->GetTextStartEndPC(&tmp1,&tmp2);    
      ldmd->SetTextStart(tmp1);
      ldmd->SetTextEnd(tmp2);     
      cout<< "\t LM text started from address : "<< hex <<"0x" << tmp1 << endl;
      cout<< "\t LM text end at the   address : "<< hex <<"0x" << tmp2 << endl; 
      cout<< "\t LM entry point is: "<< hex << "0x" <<  ldmd->GetTextStart() << endl;
      cout<< "\t data file mapaddress  from   address : "<< hex <<"0x" << startaddr << endl;
      cout<< "\t data file mapaddress  supposed end   : "<< hex <<"0x" << endaddr   <<dec<< endl; 
      // if ldmd is not an excutable, do relocate  
#endif   
      if ( !(ldmd->GetType() == binutils::LM::Executable) ) {
	ldmd->Relocate(startaddr);   
      }     
      
      AddSourceFileInfoToCSProfile(profData, ldmd,startaddr,endaddr,lastone);
      
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
	
      NormalizeInternalCallSites(profData, ldmd,startaddr, endaddr,lastone);
	
      endaddr = startaddr-1;
     
      // close current load module and free memory space 
      delete ldmd;
      
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
    
    copySourceFiles (profData, args.searchPaths, dbSourceDirectory);
    
    WriteCSProfileInDatabase (profData, args.dbDir);
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
