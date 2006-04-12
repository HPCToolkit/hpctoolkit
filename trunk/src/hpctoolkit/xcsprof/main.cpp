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
//    main.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
#include <fstream>
#include <new>
#include <sys/stat.h>
#include <sys/types.h>

//*************************** User Include Files ****************************

#include "Args.h"
//#include "ProfileReader.h" FIXME
#include "CSProfileUtils.h"
#include <lib/binutils/LoadModule.h>
#include <lib/binutils/PCToSrcLineMap.h>
#include <lib/binutils/LoadModuleInfo.h>

//*************************** Forward Declarations ***************************

using std::cerr;

//****************************************************************************

#if 0
#define DEB_ON_LINUX 1  
#endif

int
main(int argc, char* argv[])
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
    //profData = TheProfileReader.ReadProfileFile(args.profFile /*filetype*/); 
    // we need to know the name of the executable 
    profData = ReadCSProfileFile_HCSPROFILE(args.profFile,args.progFile);

    if (!profData) { exit(1); }
  } catch (std::bad_alloc& x) {
    cerr << "Error: Memory alloc failed while reading profile!\n";
    exit(1);
  } catch (...) {
    cerr << "Error: Exception encountered while reading profile!\n";
    exit(2);
  }

  // ------------------------------------------------------------
  // Read 'PCToSrcLineXMap', if available
  // ------------------------------------------------------------ 
  PCToSrcLineXMap* map = NULL;
  
  // cf. xprof, main.C: During program structure recovery it is
  // possible to output source line info supplemented with, e.g., loop
  // nesting info.  While this is not yet fully supported, we may want
  // to use something similar in the future.
  
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
    LoadModule*      ldmd   = NULL; 
    CSProfLDmodule * csploadmd= NULL; 
    LoadModuleInfo * modInfo;

    for (int i=0; i<numberofldmd; i++) {  
       csploadmd = profData->GetEpoch()->GetLdModule(i);    

       if (!(csploadmd->GetUsedFlag())) //ignore unused loadmodule 
          continue;

       try {
         ldmd = new LoadModule();  
         if (!ldmd ->Open(csploadmd->GetName()))   //Error message already printed
            continue; 
         if (!ldmd->Read())   //Error message already printed
            continue;    
          } catch (std::bad_alloc& x) {
            cerr << "Error: Memory alloc failed while reading binary!\n";
            exit(1);
          } catch (...) {
            cerr << "Error: Exception encountered while reading binary!\n";
            exit(2);
          }

       modInfo = new LoadModuleInfo(ldmd, map);  

       AddSourceFileInfoToCSProfile(profData, modInfo); 

       NormalizeCSProfile(profData);
    
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

      NormalizeInternalCallSites(profData, modInfo);   
     
      // close current load module and free memory space 
      delete(modInfo->GetLM()); 
      modInfo = NULL;

    } /* for each load module */

    // prepare output directory 
    
    /*
      FIXME:
      if (db exists) {
      create other one, dbname+randomNumber until new name found
      create xml file (name csprof.xml)
      create src dir
      }
    */

    String dbSourceDirectory = args.databaseDirectory+"/src";
    if (mkdir(dbSourceDirectory,
	      S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
      cerr << "could not create database source code directory " << 
	dbSourceDirectory << endl;
      exit (1);
    }
    
    copySourceFiles (profData, args.searchPaths, dbSourceDirectory);

    WriteCSProfileInDatabase (profData, args.databaseDirectory);
    //WriteCSProfile(profData, std::cout, /* prettyPrint */ true);
    
    
  } catch (...) { // FIXME
    cerr << "Error: Exception encountered while preparing CSPROFILE!\n";
    exit(2);
  } 

#if 0  /* already deleted */
  delete exe; 
  delete map;
#endif 
  delete profData;
  return (0);
}

//****************************************************************************
