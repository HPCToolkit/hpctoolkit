// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//    $HeadURL$
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
#include <lib/support/RealPathMgr.hpp>
#include <lib/support/StrUtil.hpp>

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

  for (Metric::SampledDescVec::iterator it = m_mdescs.begin();
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
    proflm->read(m_fs, m_name.c_str());
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
  proflm->read(m_fs, m_name.c_str());

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
    m_mdescs[i] = new Metric::SampledDesc(profevent.mdesc());
  }
}


FILE*
ProfileData::fopen(const char* filename)
{
  struct stat statbuf;
  int ret = stat(filename, &statbuf);
  if (ret != 0) {
    PROFFLAT_Throw("Cannot stat file: '" << filename << "'");
  }

  FILE* fs = ::fopen(filename, "r");
  if (!fs) {
    PROFFLAT_Throw("Cannot open file: '" << filename << "'");
  }

  return fs;
}


void
ProfileData::read_header(FILE* fs)
{
  char magic_str[HPCRUNFLAT_FMT_MagicLen];
  char version[HPCRUNFLAT_VersionLen];
  char endian;
  int c;
  size_t sz;

  sz = fread((char*)magic_str, 1, HPCRUNFLAT_FMT_MagicLen, fs);
  if (sz != HPCRUNFLAT_FMT_MagicLen) {
    PROFFLAT_Throw("Error reading <header>.");
  }
  
  sz = fread((char*)version, 1, HPCRUNFLAT_VersionLen, fs);
  if (sz != HPCRUNFLAT_VersionLen) {
    PROFFLAT_Throw("Error reading <header>.");
  }
  
  if ((c = fgetc(fs)) == EOF) {
    PROFFLAT_Throw("Error reading <header>.");
  }
  endian = (char)c;
  

  // sanity check header
  if (strncmp(magic_str, HPCRUNFLAT_FMT_Magic,
	      HPCRUNFLAT_FMT_MagicLen) != 0) {
    PROFFLAT_Throw("Error reading <header>: bad magic string.");
  }
  if (strncmp(version, HPCRUNFLAT_Version, HPCRUNFLAT_VersionLen) != 0) {
    PROFFLAT_Throw("Error reading <header>: bad version.");
  }
  if (endian != HPCRUNFLAT_FMT_Endian) {
    PROFFLAT_Throw("Error reading <header>: bad endianness.");
  }
}


uint
ProfileData::read_lm_count(FILE* fs)
{
  uint32_t count;

  size_t sz = hpcio_le4_fread(&count, fs);
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
LM::read(FILE *fs, const char* filename)
{
  size_t sz;

  // -------------------------------------------------------
  // <loadmodule_name>, <loadmodule_loadoffset>
  // -------------------------------------------------------
  if (read_string(fs, m_name) != 0) {
    PROFFLAT_Throw("Error reading <loadmodule_name>.");
  }
  RealPathMgr::singleton().realpath(m_name);
  
  sz = hpcio_le8_fread(&m_load_addr, fs);
  if (sz != sizeof(m_load_addr)) {
    PROFFLAT_Throw("Error reading <loadmodule_loadoffset>.");
  }

  DIAG_Msg(5, "Reading: " << m_name << " loaded at 0x"
	   << hex << m_load_addr << dec);

  // -------------------------------------------------------
  // <loadmodule_eventcount>
  // -------------------------------------------------------
  uint count = 1;
  sz = hpcio_le4_fread(&count, fs);
  if (sz != sizeof(count)) {
    PROFFLAT_Throw("Error reading <loadmodule_eventcount>.");
  }
  m_eventvec.resize(count);
  
  // -------------------------------------------------------
  // Event data
  // -------------------------------------------------------
  for (uint i = 0; i < count; ++i) {
    EventData& edata = m_eventvec[i];
    edata.read(fs, m_load_addr);

    Metric::SampledDesc& mdesc = edata.mdesc();
    if (filename) {
      mdesc.profileName(filename);
    }
    mdesc.profileRelId(StrUtil::toStr(i));
    mdesc.profileType("HPCRUN");
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
  
  sz = hpcio_le8_fread(&period, fs);
  if (sz != sizeof(period)) {
    PROFFLAT_Throw("Error reading <event_x_period>.");
  }
  
  m_mdesc.nameBase(name);
  m_mdesc.description(desc);
  m_mdesc.period(period);
  //m_mdesc.flags();


  // -------------------------------------------------------
  // <event_x_data>
  // -------------------------------------------------------
  m_sparsevec.clear();
  m_outofrange = 0;
  m_overflow = 0;
  
  // <histogram_non_zero_bucket_count>
  uint64_t ndat;    // number of profile entries
  sz = hpcio_le8_fread(&ndat, fs);
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
    sz = hpcio_le4_fread(&count, fs);        // count
    if (sz != sizeof(count)) {
      PROFFLAT_Throw("Error reading <histogram_non_zero_bucket_x_value>.");
    }

    sz = hpcio_le8_fread(&offset, fs);       // offset
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
  sz = hpcio_le4_fread(&len, fs);
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
