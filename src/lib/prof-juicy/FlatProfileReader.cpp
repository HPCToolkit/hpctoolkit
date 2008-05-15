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
#include <lib/support/Logic.hpp>

//*************************** Forward Declarations **************************

//#define PROFREADER_TEST

using namespace std;

static int
read_string(FILE *fp, std::string& str);


//***************************************************************************

Prof::Flat::Profile::Profile()
  : m_fs(NULL)
{
}


Prof::Flat::Profile::~Profile()
{
  if (m_fs) {
    fclose(m_fs);
  }
  for (const_iterator it = begin(); it != end(); ++it) {
    const Prof::Flat::LM* proflm = it->second;
    delete proflm;
  }
  for (SampledMetricDescVec::iterator it = m_mdescs.begin();
       it != m_mdescs.end(); ++it) {
    delete (*it);
  }
}



void
Prof::Flat::Profile::open(const char* filename)
{
  DIAG_Assert(Logic::implies(!m_name.empty(), m_name.c_str() == filename), "Cannot open a different file!");

  // Open file
  m_fs = Profile::fopen(filename);
  m_name = filename;  

  // Gather metrics
  read_metrics();
  fseek(m_fs, 0L, SEEK_SET); // rewind
}


void
Prof::Flat::Profile::read(const char* filename)
{
  // Open file
  m_fs = Profile::fopen(filename);
  m_name = filename;  

  // Read it
  read();

  // Gather metrics
  iterator it = begin();
  mdescs(it->second);
}


void
Prof::Flat::Profile::read()
{
  // INVARIANT: at least one LM must exist in a profile.

  DIAG_Assert(m_fs, "No open file stream!");

  // -------------------------------------------------------
  // <header>
  // -------------------------------------------------------
  read_header(m_fs);

  // -------------------------------------------------------
  // <loadmodule_list>
  // -------------------------------------------------------
  uint count = read_lm_count(m_fs);
  DIAG_Assert(count > 0, "At least one LM must exist in a profile!");
  
  for (uint i = 0; i < count; ++i) {
    Prof::Flat::LM* proflm = new Prof::Flat::LM();
    proflm->read(m_fs);
    pair<iterator,bool> ret = insert(make_pair(proflm->name(), proflm));
    // FIXME: must merge if duplicate is found
    DIAG_Assert(ret.second, "Load module already exists:" << proflm->name());
  }

  fclose(m_fs);
  m_fs = NULL;
}


void
Prof::Flat::Profile::read_metrics()
{
  // ASSUMES: Each LM has the *same* set of metrics

  DIAG_Assert(m_fs, "No open file stream!");

  // -------------------------------------------------------
  // <header>
  // -------------------------------------------------------
  read_header(m_fs);

  // -------------------------------------------------------
  // read one load module
  // -------------------------------------------------------
  uint count = read_lm_count(m_fs);
  DIAG_Assert(count > 0, "At least one LM must exist in a profile!");

  Prof::Flat::LM* proflm = new Prof::Flat::LM();
  proflm->read(m_fs);

  // -------------------------------------------------------
  // gather metrics
  // -------------------------------------------------------
  mdescs(proflm);

  delete proflm;
}


void
Prof::Flat::Profile::mdescs(LM* proflm)
{
  // ASSUMES: Each LM has the *same* set of metrics

  uint sz = proflm->num_events();

  m_mdescs.resize(sz);

  for (uint i = 0; i < sz; ++i) {
    const Prof::Flat::EventData& profevent = proflm->event(i);
    m_mdescs[i] = new SampledMetricDesc(profevent.mdesc());
  }
}


FILE*
Prof::Flat::Profile::fopen(const char* filename)
{
  struct stat statbuf;
  int ret = stat(filename, &statbuf);
  if (ret != 0) {
    PROFFLAT_Throw("Cannot stat file:" << filename);
  }

  FILE* fs = ::fopen(filename, "r");
  if (!fs) {
    PROFFLAT_Throw("Cannot open file:" << filename);
  }

  return fs;
}


void
Prof::Flat::Profile::read_header(FILE* fs)
{
  char magic_str[HPCRUNFILE_MAGIC_STR_LEN];
  char version[HPCRUNFILE_VERSION_LEN];
  char endian;
  int c;
  size_t sz;

  sz = fread((char*)magic_str, 1, HPCRUNFILE_MAGIC_STR_LEN, fs);
  if (sz != HPCRUNFILE_MAGIC_STR_LEN) { 
    PROFFLAT_Throw("Error reading <header>.");
  }
  
  sz = fread((char*)version, 1, HPCRUNFILE_VERSION_LEN, fs);
  if (sz != HPCRUNFILE_VERSION_LEN) { 
    PROFFLAT_Throw("Error reading <header>.");
  }
  
  if ((c = fgetc(fs)) == EOF) { 
    PROFFLAT_Throw("Error reading <header>.");
  }
  endian = (char)c;
  

  // sanity check header
  if (strncmp(magic_str, HPCRUNFILE_MAGIC_STR, 
	      HPCRUNFILE_MAGIC_STR_LEN) != 0) { 
    PROFFLAT_Throw("Error reading <header>: bad magic string.");
  }
  if (strncmp(version, HPCRUNFILE_VERSION, HPCRUNFILE_VERSION_LEN) != 0) { 
    PROFFLAT_Throw("Error reading <header>: bad version.");
  }
  if (endian != HPCRUNFILE_ENDIAN) { 
    PROFFLAT_Throw("Error reading <header>: bad endianness.");
  }
}


uint
Prof::Flat::Profile::read_lm_count(FILE* fs)
{
  uint32_t count;

  size_t sz = hpc_fread_le4(&count, fs);
  if (sz != sizeof(count)) { 
    PROFFLAT_Throw("Error reading <loadmodule_list>.");
  }
  
  return count;
}


void
Prof::Flat::Profile::dump(std::ostream& o, const char* pre) const
{
  string p = pre;
  string p1 = p + "  ";

  o << p << "--- Profile Dump ---" << endl;
  o << p << "{ Profile: " << m_name << " }" << endl;

  for (const_iterator it = begin(); it != end(); ++it) {
    const Prof::Flat::LM* proflm = it->second;
    proflm->dump(o, p1.c_str());
  }
  o << p << "--- End Profile Dump ---" << endl;
}


//***************************************************************************

Prof::Flat::LM::LM()
{
}


Prof::Flat::LM::~LM()
{
}


void
Prof::Flat::LM::read(FILE *fs)
{
  size_t sz;

  // -------------------------------------------------------
  // <loadmodule_name>, <loadmodule_loadoffset>
  // -------------------------------------------------------
  if (read_string(fs, m_name) != 0) { 
    PROFFLAT_Throw("Error reading <loadmodule_name>.");
  }
  
  sz = hpc_fread_le8(&m_load_addr, fs);
  if (sz != sizeof(m_load_addr)) { 
    PROFFLAT_Throw("Error reading <loadmodule_loadoffset>.");
  }

  DIAG_Msg(5, "Reading: " << m_name << " loaded at 0x" 
	   << hex << m_load_addr << dec);

  // -------------------------------------------------------  
  // <loadmodule_eventcount>
  // -------------------------------------------------------
  uint count = 1;
  sz = hpc_fread_le4(&count, fs);
  if (sz != sizeof(count)) { 
    PROFFLAT_Throw("Error reading <loadmodule_eventcount>.");
  }
  m_eventvec.resize(count);
  
  // -------------------------------------------------------
  // Event data
  // -------------------------------------------------------
  for (uint i = 0; i < count; ++i) {
    m_eventvec[i].read(fs, m_load_addr);
  }
}


void
Prof::Flat::LM::dump(std::ostream& o, const char* pre) const
{
  string p = pre;
  string p1 = p + "  ";

  o << p << "{ LM: " << m_name << ", loadAddr: 0x" << hex 
    << m_load_addr << dec << " }" << endl;
  
  for (uint i = 0; i < num_events(); ++i) {
    const EventData& profevent = event(i);
    profevent.dump(o, p1.c_str());
  }
}


//***************************************************************************


Prof::Flat::EventData::EventData()
{
}


Prof::Flat::EventData::~EventData()
{
}


void
Prof::Flat::EventData::read(FILE *fs, uint64_t load_addr)
{
  size_t sz;
  
  std::string name, desc;
  uint64_t period;
  
  // -------------------------------------------------------
  // <event_x_name> <event_x_description> <event_x_period>
  // -------------------------------------------------------
  if (read_string(fs, name) != 0) { 
    PROFFLAT_Throw("Error reading <event_x_name>.");
  }
  if (read_string(fs, desc) != 0) { 
    PROFFLAT_Throw("Error reading <event_x_description>.");
  }
  
  sz = hpc_fread_le8(&period, fs);
  if (sz != sizeof(period)) { 
    PROFFLAT_Throw("Error reading <event_x_period>.");
  }
  
  m_mdesc.name(name);
  m_mdesc.description(desc);
  m_mdesc.period(period);

  // -------------------------------------------------------
  // <event_x_data>
  // -------------------------------------------------------
  m_sparsevec.clear();
  m_outofrange = 0;
  m_overflow = 0;
  
  // <histogram_non_zero_bucket_count>
  uint64_t ndat;    // number of profile entries
  sz = hpc_fread_le8(&ndat, fs);
  if (sz != sizeof(ndat)) { 
    PROFFLAT_Throw("Error reading <histogram_non_zero_bucket_count>.");
  }
  m_sparsevec.resize(ndat);

  DIAG_Msg(6, "  EventData: " << name << ": " << ndat << " entries (cnt,offset)");

  // <histogram_non_zero_bucket_x_value> 
  // <histogram_non_zero_bucket_x_offset>
  uint32_t count;   // profile count
  uint64_t offset;  // offset from load address
  for (uint i = 0; i < ndat; ++i) {
    sz = hpc_fread_le4(&count, fs);        // count
    if (sz != sizeof(count)) { 
      PROFFLAT_Throw("Error reading <histogram_non_zero_bucket_x_value>.");
    }

    sz = hpc_fread_le8(&offset, fs);       // offset
    if (sz != sizeof(offset)) { 
      PROFFLAT_Throw("Error reading <histogram_non_zero_bucket_x_offset>.");
    }
    DIAG_Msg(7, "    " << i << ": (" << count << ", " << offset << ")");
    
    VMA pc = load_addr + offset;
    m_sparsevec[i] = make_pair(pc, count);
  }
}


void
Prof::Flat::EventData::dump(std::ostream& o, const char* pre) const
{
  string p = pre;
  string p1 = p + "  ";

  o << p << "{ EventData: " << mdesc().name() 
    << ", period: " << mdesc().period() 
    << ", outofrange: " << outofrange() 
    << ", overflow: " << overflow()
    << " }" << endl;
  
  for (uint i = 0; i < num_data(); ++i) {
    const Datum& dat = datum(i);
    o << p1 << "{ 0x" << hex << dat.first << ": " << dec
      << dat.second << " }" << endl;
  }
}


//***************************************************************************

static int
read_string(FILE *fs, std::string& str)
{
  size_t sz;
  uint32_t len; // string length  
  int c;

  // <string_length> <string_without_terminator>
  sz = hpc_fread_le4(&len, fs);
  if (sz != sizeof(len)) { 
    return 1;
  }
  
  str.resize(len);
  for (uint n = 0; n < len; ++n) { 
    if ((c = fgetc(fs)) == EOF) { 
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
  Profile f;

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
