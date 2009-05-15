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
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
#include <string>

#include <sys/stat.h>

#include <cstdio>
#include <cstring>

//*************************** User Include Files ****************************

#include "Flat-ProfileData.hpp"

#include <lib/prof-lean/hpcrunflat-fmt.h>
#include <lib/prof-lean/hpcio.h>

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>

//*************************** Forward Declarations **************************

//#define PROFREADER_TEST

using namespace std;

static int
read_string(FILE *fp, std::string& str);


//***************************************************************************

namespace Prof {

namespace Flat {


ProfileData::ProfileData(const char* filename)
  : m_name((filename) ? filename : ""), m_fs(NULL)
{
}


ProfileData::~ProfileData()
{
  if (m_fs) {
    fclose(m_fs);
  }

  for (const_iterator it = begin(); it != end(); ++it) {
    const LM* proflm = it->second;
    delete proflm;
  }
  clear();

  for (SampledMetricDescVec::iterator it = m_mdescs.begin();
       it != m_mdescs.end(); ++it) {
    delete (*it);
  }
  m_mdescs.clear();
}


void
ProfileData::openread(const char* filename)
{
  DIAG_Assert(Logic::implies(!m_name.empty() && filename, m_name.c_str() == filename), "Cannot open a different file!");
  if (m_name.empty() && filename) {
    m_name = filename;
  }

  // Open file
  m_fs = ProfileData::fopen(m_name.c_str());

  // Read it
  read();

  // Gather metrics
  iterator it = begin();
  mdescs(it->second);
}


void
ProfileData::open(const char* filename)
{
  DIAG_Assert(Logic::implies(!m_name.empty() && filename, m_name.c_str() == filename), "Cannot open a different file!");
  if (m_name.empty() && filename) {
    m_name = filename;
  }

  // Open file
  m_fs = ProfileData::fopen(m_name.c_str());

  // Gather metrics
  read_metrics();
  fseek(m_fs, 0L, SEEK_SET); // rewind

  // NOT YET READ
}


void
ProfileData::read()
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
    LM* proflm = new LM();
    proflm->read(m_fs);
    insert(make_pair(proflm->name(), proflm));
  }

  fclose(m_fs);
  m_fs = NULL;
}


void
ProfileData::read_metrics()
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

  LM* proflm = new LM();
  proflm->read(m_fs);

  // -------------------------------------------------------
  // gather metrics
  // -------------------------------------------------------
  mdescs(proflm);

  delete proflm;
}


void
ProfileData::mdescs(LM* proflm)
{
  // ASSUMES: Each LM has the *same* set of metrics

  uint sz = proflm->num_events();

  m_mdescs.resize(sz);

  for (uint i = 0; i < sz; ++i) {
    const EventData& profevent = proflm->event(i);
    m_mdescs[i] = new SampledMetricDesc(profevent.mdesc());
  }
}


FILE*
ProfileData::fopen(const char* filename)
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
ProfileData::read_header(FILE* fs)
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
ProfileData::read_lm_count(FILE* fs)
{
  uint32_t count;

  size_t sz = hpcio_fread_le4(&count, fs);
  if (sz != sizeof(count)) { 
    PROFFLAT_Throw("Error reading <loadmodule_list>.");
  }
  
  return count;
}


void
ProfileData::dump(std::ostream& o, const char* pre) const
{
  string p = pre;
  string p1 = p + "  ";

  o << p << "--- ProfileData Dump ---" << endl;
  o << p << "{ ProfileData: " << m_name << " }" << endl;

  for (const_iterator it = begin(); it != end(); ++it) {
    const LM* proflm = it->second;
    proflm->dump(o, p1.c_str());
  }
  o << p << "--- End ProfileData Dump ---" << endl;
}


//***************************************************************************

LM::LM()
{
}


LM::~LM()
{
}


void
LM::read(FILE *fs)
{
  size_t sz;

  // -------------------------------------------------------
  // <loadmodule_name>, <loadmodule_loadoffset>
  // -------------------------------------------------------
  if (read_string(fs, m_name) != 0) { 
    PROFFLAT_Throw("Error reading <loadmodule_name>.");
  }
  
  sz = hpcio_fread_le8(&m_load_addr, fs);
  if (sz != sizeof(m_load_addr)) { 
    PROFFLAT_Throw("Error reading <loadmodule_loadoffset>.");
  }

  DIAG_Msg(5, "Reading: " << m_name << " loaded at 0x" 
	   << hex << m_load_addr << dec);

  // -------------------------------------------------------  
  // <loadmodule_eventcount>
  // -------------------------------------------------------
  uint count = 1;
  sz = hpcio_fread_le4(&count, fs);
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
LM::dump(std::ostream& o, const char* pre) const
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


EventData::EventData()
{
}


EventData::~EventData()
{
}


void
EventData::read(FILE *fs, uint64_t load_addr)
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
  
  sz = hpcio_fread_le8(&period, fs);
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
  sz = hpcio_fread_le8(&ndat, fs);
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
    sz = hpcio_fread_le4(&count, fs);        // count
    if (sz != sizeof(count)) { 
      PROFFLAT_Throw("Error reading <histogram_non_zero_bucket_x_value>.");
    }

    sz = hpcio_fread_le8(&offset, fs);       // offset
    if (sz != sizeof(offset)) { 
      PROFFLAT_Throw("Error reading <histogram_non_zero_bucket_x_offset>.");
    }
    DIAG_Msg(7, "    " << i << ": (" << count << ", " << offset << ")");
    
    VMA pc = load_addr + offset;
    m_sparsevec[i] = make_pair(pc, count);
  }
}


void
EventData::dump(std::ostream& o, const char* pre) const
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

} // namespace Flat

} // namespace Prof


//***************************************************************************

static int
read_string(FILE *fs, std::string& str)
{
  size_t sz;
  uint32_t len; // string length  
  int c;

  // <string_length> <string_without_terminator>
  sz = hpcio_fread_le4(&len, fs);
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
  ProfileData f;

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

//***************************************************************************
