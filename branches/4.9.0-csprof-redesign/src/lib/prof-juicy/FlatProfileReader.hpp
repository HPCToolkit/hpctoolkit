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
#include <string>
#include <utility>

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include <sys/time.h>

//*************************** User Include Files ****************************

#include <include/general.h>

#include "FlatProfileReader.hpp"

#include <lib/isa/ISATypes.hpp>

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations **************************


//***************************************************************************

// FIXME FIXME FIXME: should incrementally read the file

// Basic format for the hpcprof data file:
//   A profile file (ProfFile) contains one or more load module sections
//   (ProfFileLM) with each load module section containing one or more
//   profiling data sets, one for each event/metric. (ProfFileEvent).

//***************************************************************************

// <VMA, count>
typedef std::pair<VMA, uint32_t> ProfFileEventDatum;

// ProfFileEvent: contains the event name, profiling period and
// profiling data for the event
class ProfFileEvent {
public:
  ProfFileEvent();
  ~ProfFileEvent();
  
  // read: Throws an exception on an error!
  void read(FILE*, uint64_t load_addr);
  
  const char* name() const { return name_.c_str(); }
  const char* description() const { return desc_.c_str(); }
  uint64_t period() const { return period_; }
  
  uint outofrange() const { return outofrange_; }
  uint overflow() const { return overflow_; }
  
  // 0 based indexing
  uint num_data() const { return dat_.size(); }
  const ProfFileEventDatum& datum(uint i) const { return dat_[i]; }
  
  void dump(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  std::string  name_;
  std::string  desc_;
  uint64_t     period_;
  uint outofrange_;
  uint overflow_;
  std::vector<ProfFileEventDatum> dat_;
};

//***************************************************************************

// ProfFileLM: Contains profiling information for a load module
class ProfFileLM {
public:
  ProfFileLM();
  ~ProfFileLM();
  
  // read: Throws an exception on an error!
  void read(FILE*);
  
  const std::string& name() const { return name_; }
  uint64_t load_addr() const { return load_addr_; }
  
  // 0 based indexing
  uint num_events() const { return eventvec_.size(); }
  const ProfFileEvent& event(uint i) const { return eventvec_[i]; }
  
  void dump(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  std::string name_;                    // module name
  uint64_t load_addr_;                  // load offset during runtime
  std::vector<ProfFileEvent> eventvec_; // events
};

//***************************************************************************

// ProfFile: Contains profilng information from one run of the profiler
class ProfFile {
public:
  ProfFile();
  ~ProfFile();
  
  ProfFile(const ProfFile& f) { DIAG_Die(DIAG_Unimplemented); }
  
  // read: Throws an exception on an error!
  void read(const std::string &filename);
  
  const std::string& name() const { return name_; }
  time_t mtime() const { return mtime_; }
  
  // 0 based indexing
  uint num_load_modules() const { return lmvec_.size(); }
  const ProfFileLM& load_module(uint i) const { return lmvec_[i]; }
  
  void dump(std::ostream& o = std::cerr, const char* pre = "") const;

private:
  std::string name_; 
  time_t mtime_;
  std::vector<ProfFileLM> lmvec_; // load modules
};

//***************************************************************************

// FlatProfileReader: 
class FlatProfileReader {
public:
  FlatProfileReader();
  ~FlatProfileReader();
  
private:
};



//***************************************************************************
// Exception
//***************************************************************************

#define FLATPROF_Throw(streamArgs) DIAG_ThrowX(FlatProfile::Exception, streamArgs)

namespace FlatProfile {

class Exception : public Diagnostics::Exception {
public:
  Exception(const std::string x, const char* filenm = NULL, uint lineno = 0)
    : Diagnostics::Exception(x, filenm, lineno)
  { }
  
  virtual std::string message() const { 
    return "[FlatProfile]: " + what();
  }

private:
};

} // namespace FlatProfile


//***************************************************************************

#endif /* prof_juicy_FlatProfileReader */
