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

#ifndef PerfMetric_h 
#define PerfMetric_h 

//************************ System Include Files ******************************

#include <string>

//************************* User Include Files *******************************

#include <include/general.h>

#include <lib/support/VectorTmpl.hpp>

//************************ Forward Declarations ******************************

class NodeRetriever; 

//****************************************************************************

class DataDisplayInfo { 
public: 
  DataDisplayInfo(const char* nam, // should not be empty
		  const char* col, // may be NULL for undefined 
		  unsigned int w, 
                  bool formtAsInt)
    : name(nam), color((col ? col : "")), width(w), formatAsInt(formtAsInt)
    { }

  ~DataDisplayInfo();

  const std::string&  Name()        const { return name; }; 
  const std::string&  Color()       const { return color; }
  unsigned int        Width()       const { return width; }
  bool                FormatAsInt() const { return formatAsInt; }
  
  std::string ToString() const; 

private:
  void SetWidth(unsigned int w) { width = w; } 
  friend class HTMLTable; 

  std::string name; 
  std::string color;  
  unsigned int width; 
  bool formatAsInt; 
}; 


class PerfMetric; 

extern bool              IsPerfDataIndex(int i); 
extern unsigned int      NumberOfPerfDataInfos(); 

#define UNDEF_METRIC_INDEX (-1)
extern int               NameToPerfDataIndex(const char* name); 
inline int               NameToPerfDataIndex(const std::string& name)
{ return NameToPerfDataIndex(name.c_str()); }
                         // returns UNDEF_METRIC_INDEX 
                         // if name is not a PerfMetric name

extern PerfMetric&  IndexToPerfDataInfo(int i);
                         // asserts out iff !IsPerfDataIndex(i)
			 
extern void ClearPerfDataSrcTable(); 


class PerfMetric {
public:
  // eraxxon: Note on 'nativeName' argument: This argument doesn't
  // have a lot of meaning except with 'FilePerfMetric';
  // ComputedPerfMetrics aren't 'native'.  Really, it would be cleaner
  // to have a type field (but then this file would be dependent on
  // 'DerivedPerfMetrics') or use runtime type identification (which
  // most compilers support now).  For now, however, since the HPCVIEW
  // metric contains a native name attribute, this is reasonable (and
  // fastest).
  
  PerfMetric(const char *name, const char *nativeName, const char* displayName,
	     bool display, bool percent, bool propComputed, bool sortBy);

  PerfMetric(const std::string& name, const std::string& nativeName, 
	     const std::string& displayName, 
	     bool display, bool percent, bool propComputed, bool sortBy);

  // constructor automatically adds new instance to PerfMetricTable
  // and sets this->perfInfoIndex to instance's index in the table
  
  virtual ~PerfMetric();       

  const std::string& Name() const          { return name; }
  const std::string& NativeName() const    { return nativeName; }
  unsigned int Index() const          { return perfInfoIndex; } 
  
  bool Display() const                { return display; }
  bool Percent() const                { return percent; }
  bool PropComputed() const           { return pcomputed; }
  bool SortBy() const                 { return sortBy; }
  void setSortBy()                    { sortBy = true; }
  
  DataDisplayInfo& DisplayInfo() const { return *dispInfo; }; 
  
  unsigned int EventsPerCount()      const; 
  void SetEventsPerCount(int eventsPerCount); 
  
  virtual void Make(NodeRetriever& ret);
  
  std::string ToString() const; 

private:
  void Ctor(const char *name, const char* displayName); 

private: 
  static VectorTmpl<PerfMetric*> PerfMetricTable; 
  friend bool              IsPerfDataIndex(int i); 
  friend unsigned int      NumberOfPerfDataInfos(); 
  friend int               NameToPerfDataIndex(const char* name); 
  friend PerfMetric&       IndexToPerfDataInfo(int i);
  friend void              ClearPerfDataSrcTable(); 

  std::string name;
  std::string nativeName;
  unsigned int eventsPerCount; 
  DataDisplayInfo *dispInfo; 
  bool display; 
  bool percent; 
  bool pcomputed; 
  bool sortBy; 
  
protected: 
  unsigned int perfInfoIndex; 
};

class MetricException {
public: 
  MetricException(const char* str) { error = str; } 
  MetricException(const std::string& str) { error = str; } 

public:
  std::string error; 
}; 

#endif 
