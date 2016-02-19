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

/******************************************************************************
 * include files
 *****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <string>

#include "code-ranges.h"
#include "function-entries.h"
#include "process-ranges.h"
#include "intervals.h"
#include "server.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <map>
#include <set>

using namespace std;


/******************************************************************************
 * types
 *****************************************************************************/

class Function {
public:
  Function(void *_address, string *_comment, bool _isvisible, int _call_count);
  void AppendComment(const string *c);
  void *address;
  string *comment;
  bool isvisible;
  int call_count;
  int operator<(Function *right);
};


/******************************************************************************
 * forward declarations
 *****************************************************************************/

static void new_function_entry(void *addr, string *comment, bool isvisible, 
			       int call_count);
static void dump_function_entry(void *addr, const char *comment);


/******************************************************************************
 * local variables 
 *****************************************************************************/

typedef map<void*,Function*> FunctionSet;
typedef set<void*> ExcludedFunctionSet;

static FunctionSet function_entries;
static ExcludedFunctionSet excluded_function_entries;

static intervals cbranges;

static long num_entries_total = 0;


/******************************************************************************
 * interface operations 
 *****************************************************************************/

// Free the function_entries map, the Function objects in the map, the
// excluded_function_entries set and cbranges intervals.
//
void
function_entries_reinit(void)
{
  FunctionSet::iterator it;

  for (it = function_entries.begin(); it != function_entries.end(); it++) {
    Function *f = it->second;
    delete f->comment;
    delete f;
  }
  function_entries.clear();
  excluded_function_entries.clear();
  cbranges.clear();
  num_entries_total = 0;
}


void
exclude_function_entry(void *addr)
{
  excluded_function_entries.insert(addr);
}


void 
dump_reachable_functions()
{
  char buffer[1024];
  FunctionSet::iterator i = function_entries.begin();

  for (; i != function_entries.end();) {
    Function *f = (*i).second;
    ++i;

    const char *name;
    if (!f->isvisible && !(f->call_count > 1) && !is_possible_fn(f->address)) continue;
    if (f->comment) {
      name = f->comment->c_str();
    }
    else {
      // inferred functions must be at least 16 bytes long
      if (i != function_entries.end()) {
        Function *nextf = (*i).second;
	if (f->call_count == 0 && 
	    (((unsigned long) nextf->address) - 
	     ((unsigned long) f->address)) < 16)  {
	  long offset = offset_for_fn(f->address);
	  if (offset == 0) {
            // if f->address lies within a valid code range, its
            // offset will be non-zero. if offset is zero, the 
            // address cannot be a valid function start.
	    continue; 
          }
	  if (!range_contains_control_flow((char *) f->address + offset, 
					   ((char *) nextf->address + 
					    offset)))
	    continue;
	}
      }
      sprintf(buffer,"stripped_%p", f->address);
      name = buffer;
    }
    dump_function_entry(f->address, name);
  }
}


void 
add_stripped_function_entry(void *addr, int call_count)
{
  // only add the function if it hasn't been specifically excluded 
  if (excluded_function_entries.find(addr) == 
      excluded_function_entries.end())  {
    add_function_entry(addr, NULL, false, call_count);
  }
}


bool 
query_function_entry(void *addr)
{
  FunctionSet::iterator it = function_entries.find(addr); 

  if (it == function_entries.end()) return false;
  else return true;
}


void 
add_function_entry(void *addr, const string *comment, bool isvisible, 
		   int call_count)
{
  FunctionSet::iterator it = function_entries.find(addr); 

  if (it == function_entries.end()) {
    new_function_entry(addr, comment ? new string(*comment) : NULL, 
		       isvisible, call_count);
  } else {
    Function *f = (*it).second;
    if (comment) {
      f->AppendComment(comment);
    } else if (f->comment) {
      f->isvisible = true;
    }
    f->call_count += call_count;
  }
}


void
entries_in_range(void *start, void *end, vector<void *> &result)
{
#ifdef DEBUG_ENTRIES_IN_RANGE
  printf("function entries for range [%p, %p]\n", start, end);
#endif
  FunctionSet::iterator it = function_entries.find(start); 
  for (; it != function_entries.end(); it++) {
    void *addr = (*it).first;
    if (addr > end) return;  
#ifdef DEBUG_ENTRIES_IN_RANGE
    printf("  %p\n", addr);
#endif
    result.push_back(addr);
  }
}


bool contains_function_entry(void *address)
{
  FunctionSet::iterator it = function_entries.find(address); 
  if (it != function_entries.end()) return true;
  else return false;
}


long num_function_entries(void)
{
  return (num_entries_total);
}


/******************************************************************************
 * private operations 
 *****************************************************************************/

//
// Write one function entry in one format: server, C or text.
//
static void
dump_function_entry(void *addr, const char *comment)
{
  num_entries_total++;

  if (server_mode()) {
    syserv_add_addr(addr, function_entries.size());
    return;
  }

  if (c_mode()) {
    if (num_entries_total > 1) {
      printf(",\n");
    }
    printf("  0x%" PRIxPTR "  /* %s */", (uintptr_t) addr, comment);
    return;
  }

  // default is text mode
  printf("0x%" PRIxPTR "    %s\n", (uintptr_t) addr, comment);
}


static void 
new_function_entry(void *addr, string *comment, bool isvisible, int call_count)
{
  Function *f = new Function(addr, comment, isvisible, call_count);
  function_entries.insert(pair<void*, Function*>(addr, f));
}


int 
is_possible_fn(void *addr)
{
  return (cbranges.contains(addr) == NULL);
}


// test if an address is within a range rather than at its start
int 
inside_protected_range(void *addr)
{
  std::pair<void *const, void *> *interval = cbranges.contains(addr);
  if (interval != NULL && (addr > interval->first)) return 1;
  return 0;
}


//
// FIXME? add finer grained segv handling here?
//
void 
add_protected_range(void *start, void *end)
{
  if (start < end) {
    cbranges.insert(start,end);
  }
}


Function::Function(void *_address, string *_comment, bool _isvisible, 
		   int _call_count) 
{ 
  address = _address; 
  comment = _comment; 
  isvisible = _isvisible; 
  call_count = _call_count;
}


void
Function::AppendComment(const string *c) 
{ 
  *comment = *comment + ", " + *c;
}


int 
Function::operator<(Function *right)
{
  return this->address < right->address;
}
