// $Id$
// -*- C++ -*-

//***************************************************************************
//
// File:
//    proffile.cc
//
// Purpose:
//    Class for reading and representing hpcrun profile data.
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

#include "hpcrun.h"
#include "proffile.h"
#include "io.h"

//*************************** Forward Declarations **************************

//#define PROFREADER_TEST

using namespace std;

static int read_string(FILE *fp, std::string& str);


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
  char magic_str[HPCRUN_MAGIC_STR_LEN];
  char version[HPCRUN_VERSION_LEN];
  char endian;
  int c;
  size_t sz;

  sz = fread((char*)magic_str, 1, HPCRUN_MAGIC_STR_LEN, fp);
  if (sz != HPCRUN_MAGIC_STR_LEN) { return 1; }
  
  sz = fread((char*)version, 1, HPCRUN_VERSION_LEN, fp);
  if (sz != HPCRUN_VERSION_LEN) { return 1; }
  
  if ((c = fgetc(fp)) == EOF) { return 1; }
  endian = (char)c;
  
  // Sanity check Header information
  if (strncmp(magic_str, HPCRUN_MAGIC_STR, HPCRUN_MAGIC_STR_LEN) != 0) { 
    return 1; 
  }
  if (strncmp(version, HPCRUN_VERSION, HPCRUN_VERSION_LEN) != 0) { 
    return 1; 
  }
  if (endian != HPCRUN_ENDIAN) { return 1; }


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

#undef  DEBUG

int
ProfFileLM::read(FILE *fp)
{
  size_t sz;
  
  // Load module name and load offset.
  if (read_string(fp, name_) != 0) { return 1; }
#ifdef DEBUG
  cerr << name_ << " "; 
#endif
  
  sz = hpc_fread_le8(&load_addr_, fp);
  if (sz != sizeof(load_addr_)) { return 1; }
#ifdef DEBUG
  cerr << "load address=" << load_addr_ << endl; 
#endif

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
}

ProfFileEvent::~ProfFileEvent()
{
}

int
ProfFileEvent::read(FILE *fp, uint64_t load_addr)
{
  size_t sz;
  
  // Profiling event name, description and period
  if (read_string(fp, name_) != 0) { return 1; }
  if (read_string(fp, desc_) != 0) { return 1; }
  
  sz = hpc_fread_le8(&period_, fp);
  if (sz != sizeof(period_)) { return 1; }
  
  // Profiling data
  dat_.clear();
  outofrange_ = 0;
  overflow_ = 0;
  
  // Profiling entry: count and offset
  unsigned int ndat;    // number of profile entries
  unsigned int count; // profile count
  unsigned int offset;  // offset from load address
  
  sz = hpc_fread_le4(&ndat, fp);
  if (sz != sizeof(ndat)) { return 1; }
#ifdef DEBUG
  cerr << "ndat =" << ndat << endl; 
#endif

  dat_.resize(ndat);
  for (unsigned int i = 0; i < ndat; ++i) {
    sz = hpc_fread_le4(&count, fp);
    if (sz != sizeof(count)) { return 1; }

    sz = hpc_fread_le4(&offset, fp);
    if (sz != sizeof(offset)) { return 1; }
#ifdef DEBUG
  cerr << "  (cnt,offset)=(" << count << "," << offset << ")" << endl; 
#endif
    
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

static int
read_string(FILE *fp, std::string& str)
{
  size_t sz;
  uint32_t len; // string length  
  int c;

  // Note: the string is not null terminated.
  sz = hpc_fread_le4(&len, fp);
  if (sz != sizeof(len)) { return 1; }
  
  str.resize(len);
  for (unsigned int n = 0; n < len; ++n) { 
    if ((c = fgetc(fp)) == EOF) { return 1; }
    str[n] = (char)c;
  } 
  return 0;
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
