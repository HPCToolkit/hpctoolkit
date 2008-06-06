// -*-Mode: C++;-*-
// $Id$

//***************************************************************************
//
// File:
//    $Source$
//
// Purpose:
//    Class for reading and representing papirun profile data.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
//    Adapted from parts of The Visual Profiler by Curtis L. Janssen
//    (vmonfile.h).
//
//***************************************************************************

#ifndef prof_juicy_FlatProfileReader
#define prof_juicy_FlatProfileReader

//************************* System Include Files ****************************

#include <vector>
#include <map>
#include <string>
#include <utility>

#include <cstdlib>
#include <cstdio>

#include <sys/time.h>

//*************************** User Include Files ****************************

#include <include/general.h>

#include "MetricDesc.hpp"

#include <lib/isa/ISATypes.hpp>

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations **************************

//***************************************************************************


// FlatProfileReader: 
class FlatProfileReader {
public:
  FlatProfileReader();
  ~FlatProfileReader();
  
private:
};

//***************************************************************************


namespace Prof {
namespace Flat {


//***************************************************************************
// Basic format for the hpcprof data file:
//   A profile file (Profile) contains one or more load module sections
//   (LM) with each load module section containing one or more
//   profiling data sets, one for each event/metric. (ProfileEvent).
//***************************************************************************

class LM;

//---------------------------------------------------------------------------
// Profile: represents flat profile information
//   INVARIANT: a load module appears once
//---------------------------------------------------------------------------

class Profile : public std::map<std::string, LM*> {
public:
  Profile(const char* filename = NULL);
  ~Profile();
  
  const std::string& name() const { return m_name; }
  
  // -------------------------------------------------------
  // LM: iterator, find/insert, etc
  // -------------------------------------------------------
  // use inherited std::map routines

  // -------------------------------------------------------
  // Metrics
  // -------------------------------------------------------
  const SampledMetricDescVec& mdescs() { return m_mdescs; }

  // -------------------------------------------------------
  // open/read: Throws an exception on an error!
  // -------------------------------------------------------
  // Two ways of using:
  //   1. open(fnm): open fnm and read metrics information only
  //      read()   : read (rest of) contents
  //
  //   2. openread(fnm): opens and reads fnm
  //
  // NOTE: in either case, fnm may be supplied by constructor

  void openread(const char* filename = NULL);
 
  void open(const char* filename = NULL);
  void read();

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  
  void dump(std::ostream& o = std::cerr, const char* pre = "") const;

private:
  Profile(const Profile& x);
  Profile& operator=(const Profile& x) { return *this; }

  void read_metrics();
  void mdescs(LM* proflm);

  static FILE* fopen(const char* filename);
  static void read_header(FILE* fs);
  static uint read_lm_count(FILE* fs);

private:
  std::string m_name;
  SampledMetricDescVec m_mdescs;

  // temporary data
  FILE* m_fs;
};


//***************************************************************************

typedef uint32_t bucketsz_t;

// <VMA, count>
typedef std::pair<VMA, bucketsz_t> Datum;


//***************************************************************************

// EventData: contains event description and profiling data
class EventData {
public:
  EventData();
  ~EventData();
  
  const SampledMetricDesc& mdesc() const { return m_mdesc; }

  int  bucket_size() const { return sizeof(bucketsz_t); }
  uint outofrange() const { return m_outofrange; }
  uint overflow() const { return m_overflow; }

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  
  // 0 based indexing
  uint num_data() const { return m_sparsevec.size(); }

  // <VMA, count> where VMA is a *relocated* VMA
  const Datum& datum(uint i) const { return m_sparsevec[i]; }
  
  // -------------------------------------------------------
  // read: Throws an exception on an error!
  // -------------------------------------------------------
  void read(FILE*, uint64_t load_addr);
  
  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  void dump(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  SampledMetricDesc m_mdesc;
  uint m_outofrange;
  uint m_overflow;
  std::vector<Datum> m_sparsevec;
};


//***************************************************************************

//---------------------------------------------------------------------------
// LM: represents flat profile information for a load module
//---------------------------------------------------------------------------

class LM {
public:
  LM();
  ~LM();

  const std::string& name() const { return m_name; }
  uint64_t load_addr() const { return m_load_addr; }
  
  // 0 based indexing
  uint num_events() const { return m_eventvec.size(); }
  const EventData& event(uint i) const { return m_eventvec[i]; }


  // -------------------------------------------------------
  // read: Throws an exception on an error!
  // -------------------------------------------------------
  void read(FILE*);

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  void dump(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  std::string m_name;            // module name
  uint64_t m_load_addr;          // load offset during runtime
  std::vector<EventData> m_eventvec; // events
};


//***************************************************************************
// Exception
//***************************************************************************

#define PROFFLAT_Throw(streamArgs) DIAG_ThrowX(Prof::Flat::Exception, streamArgs)

class Exception : public Diagnostics::Exception {
public:
  Exception(const std::string x, const char* filenm = NULL, uint lineno = 0)
    : Diagnostics::Exception(x, filenm, lineno)
  { }
  
  virtual std::string message() const { 
    return "[Prof::Flat]: " + what();
  }

private:
};

//***************************************************************************

} // namespace Flat
} // namespace Prof


//***************************************************************************

#endif /* prof_juicy_FlatProfileReader */
