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
 * File:   RemoteTraceAnalyzer.cpp
 * Author: Lai Wei <lai.wei@rice.edu>
 * 
 * Created on August 5, 2018, 11:23 AM
 */

#include <lib/trace-analysis/data/TCT-Serialization.hpp>
#include <lib/trace-analysis/TraceAnalysisCommon.hpp>
#include <lib/trace-analysis/ClockSynchronizer.hpp>

#include "RemoteTraceAnalyzer.hpp"

#include <sstream>
using std::stringstream;

#include <mpi.h>

namespace TraceAnalysis {
  RemoteTraceAnalyzer::RemoteTraceAnalyzer() {}

  RemoteTraceAnalyzer::~RemoteTraceAnalyzer() {}
  
  string binaryAnalyzerToString(const BinaryAnalyzer* binaryAnalyzer) {
    std::stringstream ss;
    binary_oarchive oa(ss);
    register_class(oa);
    oa << binaryAnalyzer;
    return ss.str();
  }
  
  BinaryAnalyzer* stringToBinaryAnalyzer(string& str) {
    BinaryAnalyzer* binaryAnalyzer = NULL;
    std::stringstream ss(str);
    binary_iarchive ia(ss);
    register_class(ia);
    ia >> binaryAnalyzer;
    return binaryAnalyzer;
  }
  
  void RemoteTraceAnalyzer::syncBinaryAnalyzer(BinaryAnalyzer*& binaryAnalyzer, int myRank, int numRanks) {
    char* buf = NULL;
    int bufSize = 0;

    // Init buf and bufSize for the rootRank.
    if (myRank == 0) {
      string str = binaryAnalyzerToString(binaryAnalyzer);
      bufSize = str.size();
      buf = new char[bufSize];
      memcpy(buf, str.c_str(), bufSize);
    }
    
    // Bcast bufSize
    MPI_Bcast(&bufSize, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Bcast buf
    if (myRank != 0)
      buf = new char[bufSize];
    MPI_Bcast(buf, bufSize, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Read node from buf
    if (myRank != 0) {
      string str(buf, bufSize);
      delete binaryAnalyzer;
      //BinaryAnalyzer* temp = stringToBinaryAnalyzer(str);
      binaryAnalyzer = stringToBinaryAnalyzer(str);
    }

    delete[] buf;
  }
  
  string rootClusterToString(const TCTClusterNode* rootCluster) {
    std::stringstream ss;
    binary_oarchive oa(ss);
    register_class(oa);
    oa << rootCluster;
    return ss.str();
  }
  
  TCTClusterNode* stringToRootCluster(string& str) {
    TCTClusterNode* rootCluster = NULL;
    std::stringstream ss(str);
    binary_iarchive ia(ss);
    register_class(ia);
    ia >> rootCluster;
    return rootCluster;
  }
  
  TCTClusterNode* RemoteTraceAnalyzer::mergeRootCluster(TCTClusterNode* rootCluster, int myRank, int numRanks) {
    // Merge rootClusters from every MPI rank using binomial tree
    int mask = 0x1;
    while (mask < numRanks) {
      if (myRank & mask) { // Need to send data to partner
        int dst = myRank - mask;
        string str = rootClusterToString(rootCluster);
        
        print_msg(MSG_PRIO_LOW, "\n\nBefore MPI_Send:\n\n%s", rootCluster->toString(10, 4000, 0).c_str());
        
        MPI_Send(str.c_str(), str.size(), MPI_CHAR, dst, 0, MPI_COMM_WORLD);
      }
      else { // Need to receive data from partner if such partner exist
        int src = myRank + mask;
        if (src < numRanks) {
          // Receive rootCluster node from partner.
          MPI_Status status;
          MPI_Probe(src, 0, MPI_COMM_WORLD, &status);
          int buf_size;
          MPI_Get_count(&status, MPI_CHAR, &buf_size);
          char* buf = new char[buf_size];
          MPI_Recv(buf, buf_size, MPI_CHAR, src, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
          string str(buf, buf_size);
          TCTClusterNode* otherRootCluster = stringToRootCluster(str);
          delete[] buf;
          str.clear();
          
          print_msg(MSG_PRIO_LOW, "\n\nAfter MPI_Recv:\n\n%s", otherRootCluster->toString(10, 4000, 0).c_str());
          
          if (otherRootCluster != NULL) {
            if (rootCluster == NULL)
              rootCluster = otherRootCluster;
            else {
              TCTClusterNode* mergedRootCluster = new TCTClusterNode(*rootCluster, *otherRootCluster);
              
              delete rootCluster;
              delete otherRootCluster;
              
              rootCluster = mergedRootCluster;
            }
          }
        }
      }
      mask <<= 1;
    }
    
    return rootCluster;
  }
  
    
  string rootToString(const TCTRootNode* root) {
    std::stringstream ss;
    binary_oarchive oa(ss);
    register_class(oa);
    oa << root;
    return ss.str();
  }
  
  TCTRootNode* stringToRoot(string& str) {
    TCTRootNode* root = NULL;
    std::stringstream ss(str);
    binary_iarchive ia(ss);
    register_class(ia);
    ia >> root;
    return root;
  }
  
  const TCTRootNode* bcastRootNode(const TCTRootNode* root, int myRank, int numRanks) {
    char* buf = NULL;
    int bufSize = 0;

    // Init buf and bufSize for the rootRank.
    if (myRank == 0) {
      string str = rootToString(root);
      bufSize = str.size();
      buf = new char[bufSize];
      memcpy(buf, str.c_str(), bufSize);
    }

    // Bcast bufSize
    MPI_Bcast(&bufSize, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Bcast buf
    if (myRank != 0)
      buf = new char[bufSize];
    MPI_Bcast(buf, bufSize, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Read node from buf
    if (myRank != 0) {
      string str(buf, bufSize);
      root = stringToRoot(str);
    }

    delete[] buf;
    return root;
  }
  
  void RemoteTraceAnalyzer::getClockDiffAndSync(const TCTRootNode*& avgRoot, const vector<TCTRootNode*>& rootNodes, 
      int myRank, int numRanks, vector<Time>& clockDiff) {
    // Step 1: bcast avgRoot to every MPI rank.
    avgRoot = bcastRootNode(avgRoot, myRank, numRanks);
    
    // Bcast the thread TCT used for clock comparison, which would be rootNodes[0] of rank #0.
    const TCTRootNode* clockCmpRootNode = NULL;
    if (myRank == 0)
      clockCmpRootNode = rootNodes[0];
    clockCmpRootNode = bcastRootNode(clockCmpRootNode, myRank, numRanks);
    
    ClockSynchronizer clocksync(avgRoot);
    for (uint idx = 0; idx < rootNodes.size(); idx++)
      clockDiff[idx] = clocksync.getClockDifferenceAndSync(clockCmpRootNode, rootNodes[idx]);
    
    if (myRank != 0)
      delete clockCmpRootNode;
  }
}

