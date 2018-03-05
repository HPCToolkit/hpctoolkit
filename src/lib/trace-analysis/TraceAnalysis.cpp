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

/* 
 * File:   TraceAnalysis.cpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on February 28, 2018, 10:59 PM
 */

#include <sys/time.h>
#include <dirent.h>
#include <stdio.h>

#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/support/FileUtil.hpp>

#include "TraceAnalysis.hpp"
#include "cfg/CFGNode.hpp"
#include "cfg/BinaryAnalyzer.hpp"
#include "CCTVisitor.hpp"

#include "tct/TCT-Node.hpp"

namespace TraceAnalysis {
  static int hpctraceFileFilter(const struct dirent* entry)
  {
    static const string ext = string(".") + HPCRUN_TraceFnmSfx;
    static const uint extLen = ext.length();

    uint nmLen = strlen(entry->d_name);
    if (nmLen > extLen) {
      int cmpbeg = (nmLen - extLen);
      return (strncmp(&entry->d_name[cmpbeg], ext.c_str(), extLen) == 0);
    }
    return 0;
  }
  
  struct timeval startTime;
  struct timeval curTime;
  
  bool analysis(Prof::CallPath::Profile* prof, vector<string> profDirs, int myRank, int numRanks) {
    if (myRank == 0) {
      printf("\nTrace analysis turned on.\n");
      
      printf("Trace analysis init started at 0.000s.\n\n");
      gettimeofday(&startTime, NULL);
      
      // Step 1: analyze binary files to get CFGs for later analysis
      BinaryAnalyzer ba;
      bool flag = true;
      
      const Prof::LoadMap* loadmap = prof->loadmap();
      for (Prof::LoadMap::LMId_t i = Prof::LoadMap::LMId_NULL;
           i <= loadmap->size(); ++i) {
        Prof::LoadMap::LM* lm = loadmap->lm(i);
        if (lm->isUsed() && lm->id() != Prof::LoadMap::LMId_NULL) {
          printf("Analyzing executable: %s\n", lm->name().c_str());
          if (flag) {
            ba.parse(lm->name());
            flag = false;
          }
        }
      }
      
      // Step 2: visit CCT to build an cpid to CCTNode map.
      CCTVisitor cctVisitor(prof->cct());
      const unordered_map<uint, Prof::CCT::ADynNode*>& cpidMap = cctVisitor.getCpidMap();
      printf("\ncpids: ");
      for (auto it = cpidMap.begin(); it != cpidMap.end(); ++it)
        printf("0x%x, ", it->first);
      printf("\n\n");
      
      // Step 3: get a list of trace files.
      printf("Trace files:\n");
      for (auto it = profDirs.begin(); it != profDirs.end(); ++it) {
        string path = *it;
        if (FileUtil::isDir(path.c_str())) {
          // ensure 'path' ends in '/'
          if (path[path.length() - 1] != '/') {
            path += "/";
          }

          struct dirent** dirEntries = NULL;
          int dirEntriesSz = scandir(path.c_str(), &dirEntries,
                                     hpctraceFileFilter, alphasort);
          if (dirEntriesSz > 0) {
            for (int i = 0; i < dirEntriesSz; ++i) {
              printf("  %s%s\n", path.c_str(), dirEntries[i]->d_name);
              free(dirEntries[i]);
            }
            free(dirEntries);
          }
        }
      }
      
      gettimeofday(&curTime, NULL);
      long timeDiff = (curTime.tv_sec - startTime.tv_sec) * 1000000
                  + curTime.tv_usec - startTime.tv_usec;
      printf("\nTrace analysis init ended at %ld.%ld%ld%lds.\n", timeDiff/1000000,
              timeDiff/100000%10, timeDiff/10000%10, timeDiff/1000%10);      
      printf("Trace analysis init ended at %ld us.\n", timeDiff);
    }
    return true;
  }
}