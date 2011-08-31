// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcfnbounds/variable-entries.cpp $
// $Id: variable-entries.cpp 3479 2011-03-27 22:37:36Z xl10 $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2011, Rice University
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

#include <map>
#include <set>

using namespace std;


/******************************************************************************
 * types
 *****************************************************************************/

class Variable {
public:
  Variable(void *_address, string *_comment, bool _isvisible);
  void AppendComment(const string *c);
  void *address;
  string *comment;
  bool isvisible;
  int operator<(Variable *right);
};


/******************************************************************************
 * forward declarations
 *****************************************************************************/

static void new_variable_entry(void *addr, string *comment, bool isvisible);
static void dump_variable_entry(void *addr, const char *comment);
static void end_of_variable_entries(void);


/******************************************************************************
 * local variables 
 *****************************************************************************/

typedef map<void*,Variable*> VariableSet;

static VariableSet variable_entries;

#define ADDR_BUF_SIZE  1024

static void *addr_buf[ADDR_BUF_SIZE];
static long num_entries_in_buf = 0;
static long num_entries_total = 0;


/******************************************************************************
 * interface operations 
 *****************************************************************************/

void 
dump_variables()
{
  VariableSet::iterator i = variable_entries.begin();

  for (; i != variable_entries.end();) {
    Variable *v = (*i).second;
    ++i;

    const char *name = NULL;
    if (v->comment) {
      name = v->comment->c_str();
    }
    dump_variable_entry(v->address, name);
  }
  end_of_variable_entries();
}

void 
add_variable_entry(void *addr, const string *comment, bool isvisible)
{
  VariableSet::iterator it = variable_entries.find(addr); 

  if (it == variable_entries.end()) {
    new_variable_entry(addr, comment ? new string(*comment) : NULL, 
		       isvisible);
  } else {
    Variable *v = (*it).second;
    if (comment) {
      v->AppendComment(comment);
    } else if (v->comment) {
      v->isvisible = true;
    }
  }
}



long num_variable_entries(void)
{
  return (num_entries_total);
}


/******************************************************************************
 * private operations 
 *****************************************************************************/

//
// Write one variable entry, possibly to multiple files.
// The binary format is buffered.
//
static void
dump_variable_entry(void *addr, const char *comment)
{
  num_entries_total++;

  if (binary_fmt_fd() >= 0) {
    addr_buf[num_entries_in_buf] = addr;
    num_entries_in_buf++;
    if (num_entries_in_buf == ADDR_BUF_SIZE) {
      write(binary_fmt_fd(), addr_buf, num_entries_in_buf * sizeof(void *));
      num_entries_in_buf = 0;
    }
  }

  if (c_fmt_fp() != NULL) {
    if (num_entries_total > 1)
      fprintf(c_fmt_fp(), ",\n");
    fprintf(c_fmt_fp(), "   %p /* %s */", addr, comment);
  }

  if (text_fmt_fp() != NULL) {
    fprintf(text_fmt_fp(), "%p    %s\n", addr, comment);
  }
}


static void
end_of_variable_entries(void)
{
  if (binary_fmt_fd() >= 0 && num_entries_in_buf > 0) {
    write(binary_fmt_fd(), addr_buf, num_entries_in_buf * sizeof(void *));
    num_entries_in_buf = 0;
  }

  if (c_fmt_fp() != NULL && num_entries_total > 0)
    fprintf(c_fmt_fp(), "\n");
}


static void 
new_variable_entry(void *addr, string *comment, bool isvisible)
{
  Variable *f = new Variable(addr, comment, isvisible);
  variable_entries.insert(pair<void*, Variable*>(addr, f));
}


Variable::Variable(void *_address, string *_comment, bool _isvisible)
{ 
  address = _address; 
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
