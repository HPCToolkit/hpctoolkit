// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
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

#include <map>
using namespace std;

#include "code-ranges.h"
#include "process-ranges.h"

/******************************************************************************
 * forward declarations 
 *****************************************************************************/


/******************************************************************************
 * types
 *****************************************************************************/

class CodeRange {
public:
  CodeRange(void *_start, void *_end, long _offset, bool discover);
  void Process();
  bool Contains(void *addr);
  bool Discover() { return discover; }
  void *Relocate(void *addr); 
  long Offset() { return offset; }
private:
  void *start;
  void *end;
  long offset;
  bool discover;
};


typedef map<void*,CodeRange*> CodeRangeSet;


/******************************************************************************
 * local variables 
 *****************************************************************************/

static CodeRangeSet code_ranges;



/******************************************************************************
 * interface operations 
 *****************************************************************************/


long
offset_for_fn(void *addr)
{
  CodeRangeSet::iterator it = code_ranges.lower_bound(addr);

  if (it != code_ranges.begin()) { 
    if (--it != code_ranges.begin()) {
      CodeRange *r = (*it).second;
      if (r->Contains(addr)) return r->Offset();
    }
  }
  return 0L; //should never get here
} 


bool 
consider_possible_fn_address(void *addr)
{
  CodeRangeSet::iterator it = code_ranges.lower_bound(addr);

  if (it == code_ranges.begin() || it == code_ranges.end()) return false;
  if (--it == code_ranges.begin()) return false;

  CodeRange *r = (*it).second;
  return r->Contains(addr) & r->Discover();
} 


void 
new_code_range(void *start, void *end, long offset, bool discover)
{
  code_ranges.insert(pair<void*,CodeRange*>(start, 
					    new CodeRange(start, end, offset, discover)));
}


void 
process_code_ranges()
{
  process_range_init();
  CodeRangeSet::iterator it = code_ranges.begin();
  for (; it != code_ranges.end(); it++) {
    CodeRange *r = (*it).second;
    r->Process();
  }
}

/******************************************************************************
 * private operations 
 *****************************************************************************/

CodeRange::CodeRange(void *_start, void *_end, long _offset, bool _discover) 
{ 
  start = _start;
  end = _end; 
  offset = _offset;
  discover = _discover;
}

void * 
CodeRange::Relocate(void *addr)
{ 
  return (void *)(offset + (char *) addr); 
}

bool 
CodeRange::Contains(void *addr)
{
  return (addr >= start) && (addr < end);
}

void 
CodeRange::Process()
{
  process_range(-offset, Relocate(start), Relocate(end), discover);
}

