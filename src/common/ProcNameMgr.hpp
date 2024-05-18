// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef ProcNameMgr_hpp
#define ProcNameMgr_hpp

//************************* System Include Files ****************************

#include <string>
#include <map>

//*************************** User Include Files ****************************


//*************************** Forward Declarations **************************

//***************************************************************************
// ProcNameMgr
//***************************************************************************

// --------------------------------------------------------------------------
// 'ProcNameMgr'
// --------------------------------------------------------------------------

class ProcNameMgr
{
public:
  ProcNameMgr() { }
  virtual ~ProcNameMgr() { }

  virtual std::string
  canonicalize(const std::string& name) = 0;

  std::string
  canonicalizeCppTemplate(const std::string& name);

};


// --------------------------------------------------------------------------
// 'CppNameMgr'
// --------------------------------------------------------------------------

class CppNameMgr : public ProcNameMgr
{
public:
  CppNameMgr() { }
  virtual ~CppNameMgr() { }

  virtual std::string
  canonicalize(const std::string& name) {
    return canonicalizeCppTemplate(name);
  }

private:
};


// --------------------------------------------------------------------------
// 'CilkNameMgr'
// --------------------------------------------------------------------------

class CilkNameMgr : public ProcNameMgr
{
public:
  CilkNameMgr() { }
  virtual ~CilkNameMgr() { }

  virtual std::string
  canonicalize(const std::string& name);

  static const std::string cilkmain;

private:

  bool
  isGenerated(const std::string& x,
              const std::string& pfx, const std::string& sfx)
  {
    bool isSane = (x.length() > (pfx.length() + sfx.length()));
    size_t sfx_pos = x.length() - sfx.length();

    // test suffix first because it fails more than the prefix comparison
    return (isSane &&
            x.compare(sfx_pos, sfx.length(), sfx) == 0 &&
            x.compare(0, pfx.length(), pfx) == 0);
  }

  std::string
  basename(const std::string& x,
           const std::string& pfx, const std::string& sfx)
  {
    // Assume: x.length() > (pfx.length() + sfx.length())
    int len = x.length() - pfx.length() - sfx.length();
    return x.substr(pfx.length(), len);
  }

private:
  static const std::string s_procSlow_pfx,   s_procSlow_sfx;
  static const std::string s_procImport_pfx, s_procImport_sfx;
  static const std::string s_procExport_pfx, s_procExport_sfx;

  static const std::string s_inletNorm_pfx, s_inletNorm_sfx;
  static const std::string s_inletFast_pfx, s_inletFast_sfx;
  static const std::string s_inletSlow_pfx, s_inletSlow_sfx;
};


//***************************************************************************

#endif // ProcNameMgr_hpp
