// $Id$
// -*- C++ -*-

//***************************************************************************
//
// File:
//    proffile.h
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

#include <stdlib.h>
#include <stdio.h>

#include <sys/time.h>

//*************************** User Include Files ****************************

#include "papiprof.h"
#include "events.h"

//*************************** Forward Declarations **************************


//***************************************************************************

// Basic format for the papiprof data file:
//   A profile file (ProfFile) contains one or more load module sections
//   (ProfFileLM) with each load module section containing one or more
//   profiling data sets, one for each event/metric. (ProfFileEvent).

//***************************************************************************

// <pc, count>
typedef std::pair<pprof_off_t, unsigned short> ProfFileEventDatum;

// ProfFileEvent: contains the event name, profiling period and
// profiling data for the event
class ProfFileEvent {
  private:
    const papi_event_t *event_;
    unsigned long period_;
    unsigned int outofrange_;
    unsigned int overflow_;
    std::vector<ProfFileEventDatum> dat_;

  public:
    ProfFileEvent();
    ~ProfFileEvent();

    // read: Return 0 on success; non-zero (1) on error.
    int read(FILE*, unsigned long load_addr);

    const char* name() const { return event_->name; }
    const papi_event_t *event() const { return event_; }
    unsigned long period() const { return period_; }

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
    std::string name_;        // module name
    unsigned long load_addr_; // load offset during runtime FIXME type 64
    std::vector<ProfFileEvent> eventvec_; // events

  public:
    ProfFileLM();
    ~ProfFileLM();

    // read: Return 0 on success; non-zero (1) on error.
    int read(FILE*);
    
    const std::string& name() const { return name_; }
    unsigned long load_addr() const { return load_addr_; }

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
