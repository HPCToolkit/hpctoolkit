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
// Copyright ((c)) 2002-2024, Rice University
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

#ifndef BANAL_GPU_GPU_CODE_SOURCE_H
#define BANAL_GPU_GPU_CODE_SOURCE_H

//***************************************************************************
// Dyninst includes
//***************************************************************************

#include <dyn_regs.h>
#include <CodeSource.h>
#include <Symtab.h>



//***************************************************************************
// HPCToolkit includes
//***************************************************************************

#include "GPUCFG.hpp"



//***************************************************************************
// begin namespaces
//***************************************************************************

namespace Dyninst {
namespace ParseAPI {



//***************************************************************************
// type declarations
//***************************************************************************

  class GPUCodeSource : public /*Symtab */ CodeSource {
 public:
  GPUCodeSource(std::vector<GPUParse::Function *> &functions,
                Dyninst::SymtabAPI::Symtab *s);
  ~GPUCodeSource() {}

 public:
  /** InstructionSource implementation **/
  virtual bool isValidAddress(const Address) const { return false; }
  virtual void* getPtrToInstruction(const Address) const { return NULL; }
  virtual void* getPtrToData(const Address) const { return NULL; }
  virtual unsigned int getAddressWidth() const { return 0; }
  virtual bool isCode(const Address) const { return false; }
  virtual bool isData(const Address) const { return false; }
  virtual bool isReadOnly(const Address) const { return false; }
  virtual Address offset() const { return 0; }
  virtual Address length() const { return 0; }
  virtual Architecture getArch() const { return Arch_cuda; }

  virtual bool nonReturning(Address /*func_entry*/) { return false; }
        virtual bool nonReturningSyscall(int /*number*/) { return false; }

        /* If the binary file type supplies per-function
         * TOC's (e.g. ppc64 Linux), override.
         */
  virtual Address getTOC(Address) const { return _table_of_contents; }

  // statistics accessor
  virtual void print_stats() const { return; }
  virtual bool have_stats() const { return false; }

  // manage statistics
  virtual void incrementCounter(const std::string& /*name*/) const { return; }
  virtual void addCounter(const std::string& /*name*/, int /*num*/) const { return; }
  virtual void decrementCounter(const std::string& /*name*/) const { return; }
  virtual void startTimer(const std::string& /*name*/) const { return; }
  virtual void stopTimer(const std::string& /*name*/) const { return; }
  virtual bool findCatchBlockByTryRange(Address /*given try address*/, std::set<Address> & /* catch start */)  const { return false; }
};



//***************************************************************************
// end namespaces
//***************************************************************************

}
}

#endif
