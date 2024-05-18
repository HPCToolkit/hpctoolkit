// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************

#ifndef BANAL_GPU_READ_CUDA_CFG_HPP
#define BANAL_GPU_READ_CUDA_CFG_HPP

//***************************************************************************
// system includes
//***************************************************************************

#include <string>

//***************************************************************************
// Dyninst includes
//***************************************************************************

#include <CodeSource.h>
#include <CodeObject.h>



//***************************************************************************
// HPCToolkit includes
//***************************************************************************

#include "../VMAInterval.hpp"
#include "../ElfHelper.hpp"



//***************************************************************************
// interface operations
//***************************************************************************

void
buildCudaGPUCFG
(
 const std::string &search_path,
 ElfFile *elfFile,
 Dyninst::SymtabAPI::Symtab *the_symtab,
 Dyninst::ParseAPI::CodeSource **code_src,
 Dyninst::ParseAPI::CodeObject **code_obj
);

#endif
