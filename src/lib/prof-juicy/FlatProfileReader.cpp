// -*-Mode: C++;-*-
// $Id$

//***************************************************************************
//
// File:
//    $Source$
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

//************************* System Include Files ****************************

#include <iostream>
#include <string>

#include <stdio.h>
#include <sys/stat.h>

//*************************** User Include Files ****************************

#include "FlatProfileReader.hpp"

#include <lib/prof-lean/hpcfile_hpcrun.h>
#include <lib/prof-lean/io.h>

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations **************************

//#define PROFREADER_TEST

using namespace std;

static int
read_string(FILE *fp, std::string& str);


//***************************************************************************

ProfFile::ProfFile()
{
  mtime_ = 0;
}


ProfFile::~ProfFile()
{
}


void
ProfFile::read(const string &filename)
{
  FILE *fp;

  mtime_ = 0;

  struct stat statbuf;
  if (stat(filename.c_str(), &statbuf)) {
    FLATPROF_Throw("Cannot stat file.");
  }
  name_ = filename;
  mtime_ = statbuf.st_mtime;

  fp = fopen(filename.c_str(), "r");
  if (!fp) {
    FLATPROF_Throw("Cannot open file.");
  }

  // <header>
  char magic_str[HPCRUNFILE_MAGIC_STR_LEN];
  char version[HPCRUNFILE_VERSION_LEN];
  char endian;
  int c;
  size_t sz;

  sz = fread((char*)magic_str, 1, HPCRUNFILE_MAGIC_STR_LEN, fp);
  if (sz != HPCRUNFILE_MAGIC_STR_LEN) { 
    FLATPROF_Throw("Error reading <header>.");
  }
  
  sz = fread((char*)version, 1, HPCRUNFILE_VERSION_LEN, fp);
  if (sz != HPCRUNFILE_VERSION_LEN) { 
    FLATPROF_Throw("Error reading <header>.");
  }
  
  if ((c = fgetc(fp)) == EOF) { 
    FLATPROF_Throw("Error reading <header>.");
  }
  endian = (char)c;
  
  // sanity check header
  if (strncmp(magic_str, HPCRUNFILE_MAGIC_STR, 
	      HPCRUNFILE_MAGIC_STR_LEN) != 0) { 
    FLATPROF_Throw("Error reading <header>: bad magic string.");
  }
  if (strncmp(version, HPCRUNFILE_VERSION, HPCRUNFILE_VERSION_LEN) != 0) { 
    FLATPROF_Throw("Error reading <header>: bad version.");
  }
  if (endian != HPCRUNFILE_ENDIAN) { 
    FLATPROF_Throw("Error reading <header>: bad endianness.");
  }


  // <loadmodule_list>
  uint32_t count;

  sz = hpc_fread_le4(&count, fp);
  if (sz != sizeof(count)) { 
    FLATPROF_Throw("Error reading <loadmodule_list>.");
  }
  
  lmvec_.resize(count);
  for (uint i = 0; i < count; ++i) {
    lmvec_[i].read(fp);
  }

  fclose(fp);
}


void
ProfFile::dump(std::ostream& o, const char* pre) const
{
  string p = pre;
  string p1 = p + "  ";

  o << p << "--- ProfFile Dump ---" << endl;
  o << p << "{ ProfFile: " << name_ << ", modtime: " << mtime_ << " }" 
    << endl;

  for (uint i = 0; i < num_load_modules(); ++i) {
    const ProfFileLM& proflm = load_module(i);
    proflm.dump(o, p1.c_str());
  }
  o << p << "--- End ProfFile Dump ---" << endl;
}


//***************************************************************************

ProfFileLM::ProfFileLM()
{
}


ProfFileLM::~ProfFileLM()
{
}


void
ProfFileLM::read(FILE *fp)
{
  size_t sz;
  
  // <loadmodule_name>, <loadmodule_loadoffset>
  if (read_string(fp, name_) != 0) { 
    FLATPROF_Throw("Error reading <loadmodule_name>.");
  }
  
  sz = hpc_fread_le8(&load_addr_, fp);
  if (sz != sizeof(load_addr_)) { 
    FLATPROF_Throw("Error reading <loadmodule_loadoffset>.");
  }

  DIAG_Msg(2, "Reading: " << name_ << " loaded at 0x" 
	   << hex << load_addr_ << dec);
  
  // <loadmodule_eventcount>
  uint count = 1;
  sz = hpc_fread_le4(&count, fp);
  if (sz != sizeof(count)) { 
    FLATPROF_Throw("Error reading <loadmodule_eventcount>.");
  }
  eventvec_.resize(count);
  
  // Event data
  for (uint i = 0; i < count; ++i) {
    eventvec_[i].read(fp, load_addr_);
  }
}


void
ProfFileLM::dump(std::ostream& o, const char* pre) const
{
  string p = pre;
  string p1 = p + "  ";

  o << p << "{ ProfFileLM: " << name_ << ", loadAddr: 0x" << hex 
    << load_addr_ << dec << " }" << endl;
  
  for (uint i = 0; i < num_events(); ++i) {
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


void
ProfFileEvent::read(FILE *fp, uint64_t load_addr)
{
  size_t sz;
  
  // <event_x_name> <event_x_description> <event_x_period>
  if (read_string(fp, name_) != 0) { 
    FLATPROF_Throw("Error reading <event_x_name>.");
  }
  if (read_string(fp, desc_) != 0) { 
    FLATPROF_Throw("Error reading <event_x_description>.");
  }
  
  sz = hpc_fread_le8(&period_, fp);
  if (sz != sizeof(period_)) { 
    FLATPROF_Throw("Error reading <event_x_period>.");
  }
  
  // <event_x_data>
  dat_.clear();
  outofrange_ = 0;
  overflow_ = 0;
  
  // <histogram_non_zero_bucket_count>
  uint64_t ndat;    // number of profile entries
  sz = hpc_fread_le8(&ndat, fp);
  if (sz != sizeof(ndat)) { 
    FLATPROF_Throw("Error reading <histogram_non_zero_bucket_count>.");
  }
  dat_.resize(ndat);

  DIAG_Msg(3, "  Event: " << name_ << ": " << ndat << " entries (cnt,offset)");

  // <histogram_non_zero_bucket_x_value> 
  // <histogram_non_zero_bucket_x_offset>
  uint32_t count;   // profile count
  uint64_t offset;  // offset from load address
  for (uint i = 0; i < ndat; ++i) {
    sz = hpc_fread_le4(&count, fp);        // count
    if (sz != sizeof(count)) { 
      FLATPROF_Throw("Error reading <histogram_non_zero_bucket_x_value>.");
    }

    sz = hpc_fread_le8(&offset, fp);       // offset
    if (sz != sizeof(offset)) { 
      FLATPROF_Throw("Error reading <histogram_non_zero_bucket_x_offset>.");
    }
    DIAG_Msg(4, "    " << i << ": (" << count << ", " << offset << ")");
    
    VMA pc = load_addr + offset;
    dat_[i] = make_pair(pc, count);
  }
}


void
ProfFileEvent::dump(std::ostream& o, const char* pre) const
{
  string p = pre;
  string p1 = p + "  ";

  o << p << "{ ProfFileEvent: " << name() << ", period: " << period() 
    << ", outofrange: " << outofrange() << ", overflow: " << overflow()
    << " }" << endl;
  
  for (uint i = 0; i < num_data(); ++i) {
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

  // <string_length> <string_without_terminator>
  sz = hpc_fread_le4(&len, fp);
  if (sz != sizeof(len)) { 
    return 1;
  }
  
  str.resize(len);
  for (uint n = 0; n < len; ++n) { 
    if ((c = fgetc(fp)) == EOF) { 
      return 1;
    }
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
  } 
  else {
    cerr << "error reading file!";
  }

  cout << "File dump:" << endl;
  f.dump(cout);
}

#endif
