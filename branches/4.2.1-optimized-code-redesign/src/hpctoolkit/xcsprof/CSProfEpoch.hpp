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
//    $Source$
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef CSProfEpoch_H
#define CSProfEpoch_H

//************************* System Include Files ****************************

#include <vector>
#include <algorithm>
#include <iostream>

//*************************** User Include Files ****************************

#include <include/general.h>

#include <lib/isa/ISATypes.hpp>

#include <lib/binutils/LoadModule.hpp>
#include <lib/binutils/LoadModuleInfo.hpp>

#include <lib/support/NonUniformDegreeTree.hpp>
#include <lib/support/Unique.hpp>
#include <lib/support/String.hpp>

//*************************** Forward Declarations ***************************



class CSProfLDmodule:public Unique {
public:
   // Constructor/Destructor
    CSProfLDmodule();

    virtual ~CSProfLDmodule();

    String  GetName() const{return name;}

    VMA GetVaddr() const {return vaddr; }

    VMA GetMapaddr() const {return mapaddr;} 
    bool GetUsedFlag() const {return used;}

    void SetName(const char* s) {name = s; }
    void SetVaddr(VMA  v) {vaddr=v;}
    void SetMapaddr(VMA  m) {mapaddr=m; }  
    void SetUsedFlag(bool b) {used=b;}
    
    bool LdMdInfoIsEmpty() {return (ldminfo==NULL); } 
    void SetLdMdInfo(LoadModuleInfo* lm) {ldminfo=lm;}
    LoadModuleInfo* GetLdMdInfo() {return ldminfo;}

    void Dump(std::ostream& o= std::cerr);
    void DDump();

private: 
  LoadModuleInfo* ldminfo;
  String name ;
  VMA vaddr  ;
  VMA mapaddr;  
  bool used   ;

} ;

typedef std::vector<CSProfLDmodule *> CSProfLDmoduleVec;
typedef CSProfLDmoduleVec::iterator   CSProfLDmoduleVecIt;
typedef CSProfLDmoduleVec::const_iterator CSProfLDmoduleVecCIt;

struct compare_ldmodule_by_mapaddr
 {
   bool operator()(const CSProfLDmodule* a, 
                       const CSProfLDmodule* b) const
    { return (a->GetMapaddr() < b->GetMapaddr());}
 };


//****************************************************************************
// CSProfEpoch
//****************************************************************************

class CSProfEpoch : public Unique {
public: 
   // Constructor/Destructor
   CSProfEpoch(const suint i);
   virtual ~CSProfEpoch();

   // Data
    void  AddLoadModule(CSProfLDmodule* ldm) {loadmoduleVec.push_back(ldm);}

    void SetLoadmodule(suint i, const CSProfLDmodule* ldm) {
	     loadmoduleVec[i] = const_cast<CSProfLDmodule*>(ldm);
     }

    void SortLoadmoduleByVMA(){
      std::sort(loadmoduleVec.begin(), loadmoduleVec.end(), 
                     compare_ldmodule_by_mapaddr());
     } 
    
   suint GetNumLdModule() {return numberofldmodule;} 

   CSProfLDmodule* GetLdModule(suint i)  {return loadmoduleVec[i];}
    
    // Debug
    void Dump(std::ostream& o = std::cerr);
    void DDump();

    CSProfLDmodule* FindLDmodule(VMA i);

    friend class CSProfEpoch_LdModuleIterator ;

protected:
private:
    suint            numberofldmodule;   
    // vector of load module with this epoch 
    // the size of the vector is numberofldmodule
    CSProfLDmoduleVec  loadmoduleVec;

};


// "CSProfEpoch_LdModuleIterator" iterator over all "CSPorfLDmodule" 
// within CSProfEpoch"
class CSProfEpoch_LdModuleIterator  
{
public :
  CSProfEpoch_LdModuleIterator(const CSProfEpoch& x):p (x) {
    Reset();
  }
 virtual ~CSProfEpoch_LdModuleIterator(){}

 CSProfLDmodule* Current() const { return (*it);}

 void operator++()    {it++;} //prefix
 void operator++(int) {++it;} //postfix

 bool IsValid() const { return it != p.loadmoduleVec.end(); }
 bool IsEmpty() const { return it == p.loadmoduleVec.end(); }

 // Reset and prepare for iteration again 
 void Reset() { it = p.loadmoduleVec.begin(); }

private:
  // Should not be used 
   CSProfEpoch_LdModuleIterator();
   CSProfEpoch_LdModuleIterator(const CSProfEpoch_LdModuleIterator& x);
   CSProfEpoch_LdModuleIterator& operator=(const CSProfEpoch_LdModuleIterator& x)
        {return *this;}

protected:
private:
  const  CSProfEpoch& p;
  CSProfLDmoduleVecCIt it;
};
#endif


