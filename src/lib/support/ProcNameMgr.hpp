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
#include <iostream>

//*************************** User Include Files ****************************

#include <include/uint.h>

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

    // test suffix first becuase it fails more than the prefix comparison
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
