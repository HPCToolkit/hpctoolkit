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


// FlatProfileReader: 
class FlatProfileReader {
public:
  FlatProfileReader();
  ~FlatProfileReader();
  
private:
};


//***************************************************************************
// Basic format for the hpcprof data file:
//   A profile file (Profile) contains one or more load module sections
//   (LM) with each load module section containing one or more
//   profiling data sets, one for each event/metric. (ProfileEvent).
//***************************************************************************

namespace Prof {
namespace Flat {


// <VMA, count>
typedef std::pair<VMA, uint32_t> Datum;

// Event: contains the event name, profiling period and
// profiling data for the event
class Event {
public:
  Event();
  ~Event();
  
  // read: Throws an exception on an error!
  void read(FILE*, uint64_t load_addr);
  
  const char* name() const { return m_name.c_str(); }
  const char* description() const { return m_desc.c_str(); }
  uint64_t    period() const { return m_period; }
  int         bucket_size() const { return sizeof(uint32_t); }
  
  uint outofrange() const { return m_outofrange; }
  uint overflow() const { return m_overflow; }
  
  // 0 based indexing
  uint num_data() const { return m_sparsevec.size(); }

  // <VMA, count> where VMA is a *relocated* VMA
  const Datum& datum(uint i) const { return m_sparsevec[i]; }
  
  void dump(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  std::string  m_name;
  std::string  m_desc;
  uint64_t     m_period;
  uint m_outofrange;
  uint m_overflow;
  std::vector<Datum> m_sparsevec;
};


//***************************************************************************


// LM: Contains profiling information for a load module
class LM {
public:
  LM();
  ~LM();
  
  // read: Throws an exception on an error!
  void read(FILE*);
  
  const std::string& name() const { return m_name; }
  uint64_t load_addr() const { return m_load_addr; }
  
  // 0 based indexing
  uint num_events() const { return m_eventvec.size(); }
  const Event& event(uint i) const { return m_eventvec[i]; }
  
  void dump(std::ostream& o = std::cerr, const char* pre = "") const;
  
private:
  std::string m_name;                    // module name
  uint64_t m_load_addr;                  // load offset during runtime
  std::vector<Event> m_eventvec; // events
};

//***************************************************************************

// Profile: Contains profilng information from one run of the profiler
class Profile {
public:
  Profile();
  ~Profile();
  
  Profile(const Profile& f) { DIAG_Die(DIAG_Unimplemented); }
  
  // read: Throws an exception on an error!
  void read(const std::string& filename);
  
  const std::string& name() const { return m_name; }
  time_t mtime() const { return m_mtime; }
  
  // 0 based indexing
  uint num_load_modules() const { return m_lmvec.size(); }
  const LM& load_module(uint i) const { return m_lmvec[i]; }
  
  void dump(std::ostream& o = std::cerr, const char* pre = "") const;

private:
  std::string m_name; 
  time_t m_mtime;
  std::vector<LM> m_lmvec; // load modules
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

} // namespace Flat
} // namespace Prof


//***************************************************************************

#endif /* prof_juicy_FlatProfileReader */
