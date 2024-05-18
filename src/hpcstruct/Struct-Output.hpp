// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

// This file defines the API for writing an hpcstruct file directly
// from the TreeNode format.

//***************************************************************************

#ifndef Banal_Struct_Output_hpp
#define Banal_Struct_Output_hpp

#include <ostream>
#include <string>

#include "../common/StringTable.hpp"

#include "Struct-Inline.hpp"
#include "Struct-Skel.hpp"

namespace BAnal {
namespace Output {

using namespace Struct;
using namespace std;

void printStructFileBegin(ostream *, ostream *, string);
void printStructFileEnd(ostream *, ostream *);

void printLoadModuleBegin(ostream *, string, bool has_calls);
void printLoadModuleEnd(ostream *);

void printFileBegin(ostream *, FileInfo *);
void printFileEnd(ostream *, FileInfo *);

void earlyFormatProc(ostream *, FileInfo *, GroupInfo *, ProcInfo *,
                     bool, HPC::StringTable & strTab);

void finalPrintProc(ostream *, ostream *, string &, string &,
                    FileInfo *, GroupInfo *, ProcInfo *);

void setPrettyPrint(bool _pretty_print_output);

void enableCallTags();

}  // namespace Output
}  // namespace BAnal

#endif
