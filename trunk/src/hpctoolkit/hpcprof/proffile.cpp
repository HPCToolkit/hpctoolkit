// $Id$
// -*- C++ -*-

//***************************************************************************
//
// File:
//    proffile.cc
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
//    (vmonfile.cc).
//
//***************************************************************************

#ifdef __GNUC__
#pragma implementation
#endif

//************************* System Include Files ****************************

#include <iostream>
#include <string>

#include <stdio.h>
#include <sys/stat.h>

//*************************** User Include Files ****************************

#include "papirun.h"
#include "proffile.h"
#include "events.h"
#include "io.h"

//*************************** Forward Declarations **************************

using namespace std;

//#define PROFREADER_TEST

//***************************************************************************

ProfFile::ProfFile()
{
  mtime_ = 0;
}

ProfFile::~ProfFile()
{
}

int
ProfFile::read(const string &filename)
{
  FILE *fp;

  mtime_ = 0;

  struct stat statbuf;
  if (stat(filename.c_str(), &statbuf)) {
    return 1;
  }
  name_ = filename;
  mtime_ = statbuf.st_mtime;

  fp = fopen(filename.c_str(), "r");

  // Read Header information
  char magic_str[PAPIRUN_MAGIC_STR_LEN];
  char version[PAPIRUN_VERSION_LEN];
  char endian;
  int c;
  size_t sz;

  sz = fread((char*)magic_str, 1, PAPIRUN_MAGIC_STR_LEN, fp);
  if (sz != PAPIRUN_MAGIC_STR_LEN) { return 1; }
  
  sz = fread((char*)version, 1, PAPIRUN_VERSION_LEN, fp);
  if (sz != PAPIRUN_VERSION_LEN) { return 1; }
  
  if ((c = fgetc(fp)) == EOF) { return 1; }
  endian = (char)c;
  
  // Sanity check Header information
  if (strncmp(magic_str, PAPIRUN_MAGIC_STR, PAPIRUN_MAGIC_STR_LEN) != 0) { 
    return 1; 
  }
  if (strncmp(version, PAPIRUN_VERSION, PAPIRUN_VERSION_LEN) != 0) { 
    return 1; 
  }
  if (endian != PAPIRUN_ENDIAN) { return 1; }


  // Read Load modules
  uint32_t count;

  sz = hpc_fread_le4(&count, fp);
  if (sz != sizeof(count)) { return 1; }
  
  lmvec_.resize(count);
  for (unsigned int i = 0; i < count; ++i) {
    if (lmvec_[i].read(fp) != 0) { return 1; }
  }

  fclose(fp);
  return 0;
}

void
ProfFile::dump(std::ostream& o, const char* pre) const
{
  string p = pre;
  string p1 = p + "  ";

  o << p << "--- ProfFile Dump ---" << endl;
  o << p << "{ ProfFile: " << name_ << ", modtime: " << mtime_ << " }" 
    << endl;

  for (unsigned int i = 0; i < num_load_modules(); ++i) {
    const ProfFileLM& proflm = load_module(i);
    proflm.dump(o, p1.c_str());
  }
}

//***************************************************************************

ProfFileLM::ProfFileLM()
{
}

ProfFileLM::~ProfFileLM()
{
}

int
ProfFileLM::read(FILE *fp)
{
  size_t sz;
  int c;
    
  // Load module name and load offset.  Note: the load module string name is
  // not null terminated.
  uint32_t namelen;   // module name length
  
  sz = hpc_fread_le4(&namelen, fp);
  if (sz != sizeof(namelen)) { return 1; }
  
  name_.resize(namelen);
  for (unsigned int n = 0; n < namelen; ++n) { 
    if ((c = fgetc(fp)) == EOF) { return 1; }
    name_[n] = (char)c;
  } 

  sz = hpc_fread_le8(&load_addr_, fp);
  if (sz != sizeof(load_addr_)) { return 1; }

  // Read event data
  unsigned int count = 1;

  eventvec_.resize(count);
  for (unsigned int i = 0; i < count; ++i) {
    if (eventvec_[i].read(fp, load_addr_) != 0) { return 1; }
  }
  
  return 0;
}

void
ProfFileLM::dump(std::ostream& o, const char* pre) const
{
  string p = pre;
  string p1 = p + "  ";

  o << p << "{ ProfFileLM: " << name_ << ", loadAddr: 0x" << hex 
    << load_addr_ << dec << " }" << endl;
  
  for (unsigned int i = 0; i < num_events(); ++i) {
    const ProfFileEvent& profevent = event(i);
    profevent.dump(o, p1.c_str());
  }
}

//***************************************************************************

ProfFileEvent::ProfFileEvent()
{
  event_ = 0;
}

ProfFileEvent::~ProfFileEvent()
{
}

int
ProfFileEvent::read(FILE *fp, uint64_t load_addr)
{
  size_t sz;
    
  // Profiling event and period
  int eventcode = -1;
  
  sz = hpc_fread_le4((uint32_t*)&eventcode, fp);
  if (sz != sizeof(eventcode)) { return 1; }
  
  event_ = papirun_event_by_code(eventcode);
  
  sz = hpc_fread_le8(&period_, fp);
  if (sz != sizeof(period_)) { return 1; }
  
  // Profiling data
  dat_.clear();
  outofrange_ = 0;
  overflow_ = 0;
  
  // Profiling entry: count and offset
  unsigned int ndat;    // number of profile entries
  unsigned short count; // profile count
  unsigned int offset;  // offset from load address
  
  sz = hpc_fread_le4(&ndat, fp);
  if (sz != sizeof(ndat)) { return 1; }

  dat_.resize(ndat);
  for (unsigned int i = 0; i < ndat; ++i) {
    sz = hpc_fread_le2(&count, fp);
    if (sz != sizeof(count)) { return 1; }

    sz = hpc_fread_le4(&offset, fp);
    if (sz != sizeof(offset)) { return 1; }
    
    pprof_off_t pc = load_addr + offset;
    dat_[i] = make_pair(pc, count);
  }

  return 0;
}

void
ProfFileEvent::dump(std::ostream& o, const char* pre) const
{
  string p = pre;
  string p1 = p + "  ";

  o << p << "{ ProfFileEvent: " << name() << ", period: " << period() 
    << ", outofrange: " << outofrange() << ", overflow: " << overflow()
    << " }" << endl;
  
  for (unsigned int i = 0; i < num_data(); ++i) {
    const ProfFileEventDatum& dat = datum(i);
    o << p1 << "{ 0x" << hex << dat.first << ": " << dec
      << dat.second << " }" << endl;
  }
}

//***************************************************************************

#ifdef PROFREADER_TEST

int main(int argc, char **argv)
{
  std::string filename = argv[1];
  ProfFile f;

  int ret = f.read(filename);

  if (ret == 0) {
    cerr << "successfully read file!";
  } else {
    cerr << "error reading file!";
  }

  cout << "File dump:" << endl;
  f.dump(cout);
}

#endif
