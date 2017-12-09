// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcvarbounds/function-entries.cpp $
// $Id: function-entries.cpp 4099 2013-02-10 20:13:32Z krentel $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2017, Rice University
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

#include "variable-entries.h"
#include "server.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <map>
#include <set>

using namespace std;


/******************************************************************************
 * types
 *****************************************************************************/

class Variable {
public:
  Variable(void *_address, long _size, string *_comment, bool _isvisible);
  void AppendComment(const string *c);
  void *address;
  long size;
  string *comment;
  bool isvisible;
  int operator<(Variable *right);
};


/******************************************************************************
 * forward declarations
 *****************************************************************************/

// this functions are defined in main.cpp
// needs to move these stuff into a 3rd party file
extern int c_mode(void);
extern int server_mode(void);


static void new_variable_entry(void *addr, long size, string *comment, bool isvisible);
static void dump_variable_entry(void *addr, long size, const char *comment);


/******************************************************************************
 * local variables
 *****************************************************************************/

typedef map<void*,Variable*> VariableSet;

static VariableSet variable_entries;

static long num_entries_total = 0;


/******************************************************************************
 * interface operations
 *****************************************************************************/

// Free the function_entries map, the Function objects in the map, the
// excluded_function_entries set and cbranges intervals.
//
void
variable_entries_reinit(void)
{
  VariableSet::iterator it;

  for (it = variable_entries.begin(); it != variable_entries.end(); it++) {
    Variable *f = it->second;
    delete f->comment;
    delete f;
  }
  variable_entries.clear();
  num_entries_total = 0;
}

void
dump_variables()
{
  VariableSet::iterator i = variable_entries.begin();

  for (; i != variable_entries.end();) {
    Variable *f = (*i).second;
    ++i;

    const char *name = NULL;
    if (f->comment) {
      name = f->comment->c_str();
    }
    dump_variable_entry(f->address, f->size, name);
  }
}


void
add_variable_entry(void *addr, long size, const string *comment, bool isvisible)
{
  if(size == 0) return;
  VariableSet::iterator it = variable_entries.find(addr);

  if (it == variable_entries.end()) {
    new_variable_entry(addr, size, comment ? new string(*comment) : NULL,
           isvisible);
  } else {
    Variable *f = (*it).second;
    if (comment) {
      f->AppendComment(comment);
    } else if (f->comment) {
      f->isvisible = true;
    }
  }
}


long
num_variable_entries(void)
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
dump_variable_entry(void *addr, long size, const char *comment)
{
  num_entries_total += 2;

  if (server_mode()) {
    syserv_add_addr(addr, 2*variable_entries.size());
    syserv_add_addr((void *)size, 2*variable_entries.size());
    return;
  }

  if (c_mode()) {
    if (num_entries_total > 2) {
      printf(",\n");
    }
    printf("  0x%" PRIxPTR "  /* %s */,\n", (uintptr_t) addr, comment);
    printf("  %ld  /* %s */", size, comment);
    return;
  }

  // default is text mode
  printf("0x%" PRIxPTR "    %s\n", (uintptr_t) addr, comment);
  printf("%ld  /* %s */\n", size, comment);
}


static void
new_variable_entry(void *addr, long size, string *comment, bool isvisible)
{
  Variable *f = new Variable(addr, size, comment, isvisible);
  variable_entries.insert(pair<void*, Variable*>(addr, f));
}


Variable::Variable(void *_address, long _size, string *_comment, bool _isvisible)
{
  address = _address;
  size = _size;
  comment = _comment;
  isvisible = _isvisible;
}


void
Variable::AppendComment(const string *c)
{
  *comment = *comment + ", " + *c;
}


int
Variable::operator<(Variable *right)
{
  return this->address < right->address;
}
