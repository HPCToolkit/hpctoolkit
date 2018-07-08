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
 * File:   TCT-Cluster.hpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on June 29, 2018, 1:29 PM
 */

#ifndef TCT_CLUSTER_HPP
#define TCT_CLUSTER_HPP

#include <vector>
using std::vector;

#include <unordered_map>
using std::unordered_map;

#include <string>
using std::string;

#include "../TraceAnalysisCommon.hpp"

namespace TraceAnalysis {
  class TCTNestedLoopSizes {
  public:
    TCTNestedLoopSizes() {}
    TCTNestedLoopSizes(const TCTNestedLoopSizes& other) : loopSizes(other.loopSizes) {}
    virtual ~TCTNestedLoopSizes() {}
    
    void addOuterMostLoopSize(int size) {
      loopSizes.push_back(size);
    }
    
    int getNumNestedLoops() {
      return loopSizes.size();
    }
    
    // Return the loop size at the given level.
    // Level for inner most loop is 0.
    // Level for outer most loop is getNumNestedLoops()-1.
    int getLoopSizeAt(int level) {
      return loopSizes[level];
    }
    
  private:
    // Records the size of nested loops. Size for inner most loop is stored at sizes[0].
    // Size of outer most loop is stored at sizes[lastElement].
    vector<int> loopSizes;
  };
  
  // (Power) Regular Section Descriptor to record members of clusters.
  class TCTClusterMemberRSD {
  public:
    TCTClusterMemberRSD(long id) : 
        nested_level(0), first_id(id), first_rsd(NULL), stride(0), length(1) {}
    TCTClusterMemberRSD(TCTClusterMemberRSD* rsd, int stride, int length) : 
        nested_level(rsd->nested_level+1), first_id(rsd->first_id), stride(stride), length(length) {
      first_rsd = rsd->duplicate();
    }
    TCTClusterMemberRSD(const TCTClusterMemberRSD& other) :
        nested_level(other.nested_level), first_id(other.first_id), stride(other.stride), length(other.length) {
      if (other.first_rsd == NULL)
        first_rsd = NULL;
      else
        first_rsd = other.first_rsd->duplicate();
    }
    virtual ~TCTClusterMemberRSD() {
      if (first_rsd != NULL) delete first_rsd;
    }
    
    TCTClusterMemberRSD* duplicate() const {
      return new TCTClusterMemberRSD(*this);
    }
    
    long getFirstID() const {
      return first_id;
    }
    
    // If the latter RSD is isomorphic to this RSD, return true;
    // return false otherwise
    bool isIsomorphic(const TCTClusterMemberRSD* latter) const;
    
    // If the latter RSD can be concatenated to this RSD, return true;
    // return false otherwise.
    bool canConcatenate(const TCTClusterMemberRSD* latter) const;
    
    // Before calling this function, user must check eligibility with canConcatenate().
    // This function concatenates the latter RSD to this RSD. The latter RSD will be deallocated upon return.
    void concatenate(TCTClusterMemberRSD* latter);
    
    string toString() const;
    
    // Depth of nested RSDs. 
    // nested_level == 0 <==> a single element;
    // nested_level == 1 <==> an RSD;
    // nested_level >= 2 <==> a PRSD (has nested RSD).
    const int nested_level; 
    
  private:
    long first_id; // ID of first member
    TCTClusterMemberRSD* first_rsd; // First nested RSD for PRSDs

    int stride;
    int length;
  };
  
  class TCTClusterMembers {
  public:
    TCTClusterMembers() : max_level(0), members(max_level+1) {}
    
    TCTClusterMembers(const TCTClusterMembers& other) : max_level(other.max_level), members(max_level+1) {
      for (int level = 0; level <= max_level; level++)
        for (int k = 0; k < (int)other.members[level].size(); k++)
          members[level].push_back(other.members[level][k]->duplicate());
    }
    
    // Build a TCTClusterMembers by merging two input TCTClusterMembers.
    // All members in the two input TCTClusterMembers will be removed.
    TCTClusterMembers(TCTClusterMembers& other1, TCTClusterMembers& other2);
    
    virtual ~TCTClusterMembers() {
      for (int level = 0; level <= max_level; level++)
        for (int k = 0; k < (int)members[level].size(); k++)
          delete members[level][k];
    }
    
    TCTClusterMembers* duplicate() const {
      return new TCTClusterMembers(*this);
    }
    
    void addMember(long id) {
      addMember(new TCTClusterMemberRSD(id));
    }

    string toString() const;
    
    void test() {
      int n = 20;
      int seq[n] = {0,1,2,3,4,5,8,10,11,12,13,14,15,18,20,21,22,23,24,25};
      for (int i = 0; i < n; i++) {
        printf("\nAdding member: %d\n", seq[i]);
        addMember(seq[i]);
        printf("  status: %s\n", toString().c_str());
      }
    }
    
  private:
    int max_level;
    
    // Stores all members of a cluster.
    // MemberRSDs with nested_level == 0 is stored at members[0];
    // MemberRSDs with nested_level == 1 is stored at members[1], etc.
    vector< vector<TCTClusterMemberRSD*> > members;
    
    void addMember(TCTClusterMemberRSD* rsd);
    void updateMember(int nested_level, int idx);
    
    void detectRSD(int nested_level, int last_idx);
    void constructRSD(int nested_level, int first_idx, int stride, int length);
    
    // Return the index of the RSD with first_id == id.
    int findID(long id, int nested_level, int min_idx, int max_idx);
    void increaseMaxLevel();
  };
}

#endif /* TCT_CLUSTER_HPP */

