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

#ifndef _proffile_h
#define _proffile_h

#ifdef __GNUG__
#pragma interface
#endif

//************************* System Include Files ****************************

#include <vector>
#include <string>
#include <utility>
#include <inttypes.h>

#include <stdlib.h>
#include <stdio.h>

#include <sys/time.h>

//*************************** User Include Files ****************************

#include "hpcprof.h"

//*************************** Forward Declarations **************************


//***************************************************************************

// Basic format for the hpcprof data file:
//   A profile file (ProfFile) contains one or more load module sections
//   (ProfFileLM) with each load module section containing one or more
//   profiling data sets, one for each event/metric. (ProfFileEvent).

//***************************************************************************

// <pc, count>
typedef std::pair<pprof_off_t, uint32_t> ProfFileEventDatum;

// ProfFileEvent: contains the event name, profiling period and
// profiling data for the event
class ProfFileEvent {
  private:
    std::string  name_;
    std::string  desc_;
    uint64_t     period_;
    unsigned int outofrange_;
    unsigned int overflow_;
    std::vector<ProfFileEventDatum> dat_;

  public:
    ProfFileEvent();
    ~ProfFileEvent();

    // read: Return 0 on success; non-zero (1) on error.
    int read(FILE*, uint64_t load_addr);

    const char* name() const { return name_.c_str(); }
    const char* description() const { return desc_.c_str(); }
    uint64_t period() const { return period_; }

    unsigned int outofrange() const { return outofrange_; }
    unsigned int overflow() const { return overflow_; }

    // 0 based indexing
    unsigned int num_data() const { return dat_.size(); }
    const ProfFileEventDatum& datum(unsigned int i) const { return dat_[i]; }

    void dump(std::ostream& o = std::cerr, const char* pre = "") const;
};

//***************************************************************************

// ProfFileLM: Contains profiling information for a load module
class ProfFileLM {
  private:
    std::string name_;                    // module name
    uint64_t load_addr_;                  // load offset during runtime
    std::vector<ProfFileEvent> eventvec_; // events

  public:
    ProfFileLM();
    ~ProfFileLM();

    // read: Return 0 on success; non-zero (1) on error.
    int read(FILE*);
    
    const std::string& name() const { return name_; }
    uint64_t load_addr() const { return load_addr_; }

    // 0 based indexing
    unsigned int num_events() const { return eventvec_.size(); }
    const ProfFileEvent& event(unsigned int i) const { return eventvec_[i]; }

    void dump(std::ostream& o = std::cerr, const char* pre = "") const;
};

//***************************************************************************

// ProfFile: Contains profilng information from one run of the profiler
class ProfFile {
  private:
    std::string name_; 
    time_t mtime_;
    std::vector<ProfFileLM> lmvec_; // load modules

  public:
    ProfFile();
    ~ProfFile();

    // read: Return 0 on success; non-zero (1) on error.
    int read(const std::string &filename);

    const std::string& name() const { return name_; }
    time_t mtime() const { return mtime_; }

    // 0 based indexing
    unsigned int num_load_modules() const { return lmvec_.size(); }
    const ProfFileLM& load_module(unsigned int i) const { return lmvec_[i]; }

    void dump(std::ostream& o = std::cerr, const char* pre = "") const;
  };

#endif
