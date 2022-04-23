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
// Copyright ((c)) 2002-2022, Rice University
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

//
//  Copyright (c) 2002-2022, Rice University.
//  See the file LICENSE for details.
//
//  Simple selection sort, shows a couple of loops, some C++
//  templates.
//

#include <iostream>
#include <list>
#include <stdlib.h>

using namespace std;

typedef list<long> llist;

int main(int argc, char** argv) {
  llist olist, nlist;
  long n, N, sum;

  N = 80000;

  if (argc > 1) {
    N = atol(argv[1]);
  }

  sum = 0;
  for (n = 1; n <= N; n += 2) {
    olist.push_front(n);
    olist.push_back(n + 1);
    sum += 2 * n + 1;
  }

  cout << "orig list:  " << N << "  " << sum << endl;

  sum = 0;
  while (olist.size() > 0) {
    auto min = olist.begin();

    for (auto it = olist.begin(); it != olist.end(); ++it) {
      if (*it < *min) {
        min = it;
      }
    }
    sum += *min;
    nlist.push_back(*min);
    olist.erase(min);
  }

  cout << "new list:   " << N << "  " << sum << endl;

  return 0;
}
