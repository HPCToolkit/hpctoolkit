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

#ifndef DerivedPerfMetrics_hpp 
#define DerivedPerfMetrics_hpp 

//************************ System Include Files ******************************

#include <string>

//************************* User Include Files *******************************

#include <include/general.h> 

#include <lib/prof-juicy/PerfMetric.hpp>

//************************* Xerces Declarations ******************************

#include <xercesc/dom/DOMNode.hpp>         /* johnmc */
using XERCES_CPP_NAMESPACE::DOMNode;

//************************ Forward Declarations ******************************

class Driver;
class MathMLExpr;

//****************************************************************************

// FIXME: relocate
class ScopeInfo;
void AccumulateMetricsFromChildren(ScopeInfo* si, int perfInfoIndex);

bool IsHPCRUNFilePerfMetric(PerfMetric* m);


class FilePerfMetric : public PerfMetric {
public: 
  FilePerfMetric(const char* nm, const char* nativeNm, const char* displayNm,
		 bool display, bool percent, bool sortBy, 
		 const char* fname, const char* ftype, Driver* driver); 
  FilePerfMetric(const std::string& nm, const std::string& nativeNm, 
		 const std::string& displayNm,
		 bool display, bool percent, bool sortBy, 
		 const std::string& fname, const std::string& ftype, 
		 Driver* driver); 

  virtual ~FilePerfMetric(); 
  
  const std::string& FileName() const { return m_file; }
  const std::string& FileType() const { return m_type; } // HPCRUN, PROFILE
  
  virtual void Make(NodeRetriever &ret);
  
  virtual std::string ToString() const; 

private: 
  void MakeHPCRUN(NodeRetriever &ret); // read the file
  void MakePROFILE(NodeRetriever &ret); // read the file

  std::string m_file;
  std::string m_type; // for later use
  Driver* m_driver;
};

class ComputedPerfMetric : public PerfMetric {
public: 
  ComputedPerfMetric(const char* nm, const char* displayNm,
		     bool display, bool percent, bool sortBy, 
		     bool propagateComputed, DOMNode *expr);
  ComputedPerfMetric(const std::string& nm, const std::string& displayNm,
		     bool display, bool percent, bool sortBy, 
		     bool propagateComputed, DOMNode *expr);

  virtual ~ComputedPerfMetric(); 

  virtual void Make(NodeRetriever &ret); // compute node by node
  
  virtual std::string ToString() const; 

private:
  void Ctor(const char* nm, DOMNode *expr);

private: 
  MathMLExpr *mathExpr; 
};

#endif 
