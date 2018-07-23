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
 * File:   TCT-Serialization.cpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on July 21, 2018, 10:58 PM
 */

#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>

#include "TCT-Serialization.hpp"

// ***************************************************************************
// Macros to generate instantiation of template functions for serialization.
// ***************************************************************************

#ifdef BOOST_ARCHIVE_TEXT_OARCHIVE_HPP || BOOST_ARCHIVE_TEXT_IARCHIVE_HPP
  #define GENERATE_SERIALIZE_AS_TEXT_TEMPLATE_INSTANTIATION(CLASS_NAME) \
    template void CLASS_NAME::serialize<text_oarchive>(text_oarchive& ar, const unsigned int version) const;
#else
  #define GENERATE_SERIALIZE_AS_TEXT_TEMPLATE_INSTANTIATION(CLASS_NAME)
#endif

#ifdef BOOST_ARCHIVE_BINARY_OARCHIVE_HPP || BOOST_ARCHIVE_BINARY_IARCHIVE_HPP
  #define GENERATE_SERIALIZE_AS_BINARY_TEMPLATE_INSTANTIATION(CLASS_NAME) \
    template void CLASS_NAME::serialize<binary_oarchive>(binary_oarchive& ar, const unsigned int version); \
    template void CLASS_NAME::serialize<binary_iarchive>(binary_iarchive& ar, const unsigned int version);
#else
  #define GENERATE_SERIALIZE_AS_BINARY_TEMPLATE_INSTANTIATION(CLASS_NAME)
#endif

#ifdef BOOST_ARCHIVE_TEXT_OARCHIVE_HPP || BOOST_ARCHIVE_TEXT_IARCHIVE_HPP
  #define GENERATE_SAVE_LOAD_AS_TEXT_TEMPLATE_INSTANTIATION(CLASS_NAME) \
    template void CLASS_NAME::save<text_oarchive>(text_oarchive& ar, const unsigned int version) const; \
    template void CLASS_NAME::load<text_iarchive>(text_iarchive& ar, const unsigned int version);
#else
  #define GENERATE_SAVE_LOAD_AS_TEXT_TEMPLATE_INSTANTIATION(CLASS_NAME)
#endif

#ifdef BOOST_ARCHIVE_BINARY_OARCHIVE_HPP || BOOST_ARCHIVE_BINARY_IARCHIVE_HPP
  #define GENERATE_SAVE_LOAD_AS_BINARY_TEMPLATE_INSTANTIATION(CLASS_NAME) \
    template void CLASS_NAME::save<binary_oarchive>(binary_oarchive& ar, const unsigned int version) const; \
    template void CLASS_NAME::load<binary_iarchive>(binary_iarchive& ar, const unsigned int version);
#else
  #define GENERATE_SAVE_LOAD_AS_BINARY_TEMPLATE_INSTANTIATION(CLASS_NAME)
#endif

#define GENERATE_SERIALIZE_TEMPLATE_INSTANTIATION(CLASS_NAME) \
  GENERATE_SERIALIZE_AS_TEXT_TEMPLATE_INSTANTIATION(CLASS_NAME) \
  GENERATE_SERIALIZE_AS_BINARY_TEMPLATE_INSTANTIATION(CLASS_NAME)

#define GENERATE_SAVE_LOAD_TEMPLATE_INSTANTIATION(CLASS_NAME) \
  GENERATE_SAVE_LOAD_AS_TEXT_TEMPLATE_INSTANTIATION(CLASS_NAME) \
  GENERATE_SAVE_LOAD_AS_BINARY_TEMPLATE_INSTANTIATION(CLASS_NAME)

#include "TCT-Time.hpp"
#include "TCT-Time-Internal.hpp"
#include "TCT-Metrics.hpp"
#include "TCT-Node.hpp"
#include "TCT-Cluster.hpp"

#include "../BinaryAnalyzer.hpp"

namespace TraceAnalysis {
  // ***************************************************************************
  // Class registration for serializing pointers.
  // ***************************************************************************
  template<class Archive>
  void register_class(Archive &ar) {
    ar.template register_type<TCTTraceTime>();
    ar.template register_type<TCTProfileTime>();
    
    ar.template register_type<TCTFunctionTraceNode>();
    ar.template register_type<TCTIterationTraceNode>();
    ar.template register_type<TCTRootNode>();
    ar.template register_type<TCTLoopNode>();
    ar.template register_type<TCTLoopClusterNode>();
    ar.template register_type<TCTProfileNode>();
  }
  #ifdef BOOST_ARCHIVE_TEXT_OARCHIVE_HPP || BOOST_ARCHIVE_TEXT_IARCHIVE_HPP
    template void register_class<text_oarchive>(text_oarchive& ar);
    template void register_class<text_iarchive>(text_iarchive& ar);
  #endif
  #ifdef BOOST_ARCHIVE_BINARY_OARCHIVE_HPP || BOOST_ARCHIVE_BINARY_IARCHIVE_HPP
    template void register_class<binary_oarchive>(binary_oarchive& ar);
    template void register_class<binary_iarchive>(binary_iarchive& ar);
  #endif
  
  // ***************************************************************************
  // Implementation of serialization functions in TCT-Time.hpp and TCT-Time-Internal.hpp
  // ***************************************************************************
  template<class Archive>
  void TCTTime::save(Archive& ar, const unsigned int version) const {
    TCTATime* tmp = (TCTATime*)ptr;
    ar & tmp;
  }
  template<class Archive>
  void TCTTime::load(Archive& ar, const unsigned int version) {
    TCTATime* tmp;
    ar & tmp;
    delete (TCTATime*)ptr;
    ptr = tmp;
  }
  GENERATE_SAVE_LOAD_TEMPLATE_INSTANTIATION(TCTTime)

  template<class Archive>
  void TCTATime::serialize(Archive& ar, const unsigned int version) {
  }
  GENERATE_SERIALIZE_TEMPLATE_INSTANTIATION(TCTATime)
  
  template<class Archive>
  void TCTTraceTime::serialize(Archive& ar, const unsigned int version) {
    ar & boost::serialization::base_object<TCTATime>(*this);
    ar & startTimeExclusive & startTimeInclusive & endTimeInclusive & endTimeExclusive;
  }
  GENERATE_SERIALIZE_TEMPLATE_INSTANTIATION(TCTTraceTime)
          
  template<class Archive>
  void TCTProfileTime::serialize(Archive& ar, const unsigned int version) {
    ar & boost::serialization::base_object<TCTATime>(*this);
    ar & minDurationInclusive & maxDurationInclusive;
  }
  GENERATE_SERIALIZE_TEMPLATE_INSTANTIATION(TCTProfileTime)
          
  // ***************************************************************************
  // Implementation of serialization functions in TCT-Metrics.hpp
  // ***************************************************************************
  template<class Archive>
  void TCTDiffScore::serialize(Archive& ar, const unsigned int version) {
    ar & inclusive & exclusive;
  }
  GENERATE_SERIALIZE_TEMPLATE_INSTANTIATION(TCTDiffScore)
  
  template<class Archive>
  void TCTPerfLossMetric::serialize(Archive& ar, const unsigned int version) {
    ar & minDuration & maxDuration & totalDuration;
  }
  GENERATE_SERIALIZE_TEMPLATE_INSTANTIATION(TCTPerfLossMetric)

  // ***************************************************************************
  // Implementation of serialization functions in TCT-Node.hpp
  // ***************************************************************************
  template<class Archive>
  void TCTID::serialize(Archive& ar, const unsigned int version) {
    ar & const_cast<int&>(id) & const_cast<int&>(procID);
  }
  GENERATE_SERIALIZE_TEMPLATE_INSTANTIATION(TCTID)
  
  template<class Archive>
  void TCTANode::save(Archive& ar, const unsigned int version) const {
    ar & const_cast<TCTID&>(id) & const_cast<VMA&>(ra);
    ar & name & depth & weight;
    ar & time & diffScore & plm;
    
    // Special handling for cfgGraph
    bool isCfgNull = (cfgGraph == NULL);
    ar & isCfgNull;
    if (!isCfgNull) {
      bool isFunc = (dynamic_cast<CFGLoop* const>(cfgGraph) == NULL);
      ar & isFunc;
      ar & cfgGraph->vma;
    }
  }
  template<class Archive>
  void TCTANode::load(Archive& ar, const unsigned int version) {
    ar & const_cast<TCTID&>(id) & const_cast<VMA&>(ra);
    ar & name & depth & weight;
    ar & time & diffScore & plm;
    
    // Special handling for cfgGraph
    bool isCfgNull;
    ar & isCfgNull;
    // If isCfgNull == true, cfgGraph has been initialized to NULL, no further actions.
    if (!isCfgNull) {
      bool isFunc;
      ar & isFunc;
      VMA vma;
      ar & vma;
      const_cast<CFGAGraph*&>(cfgGraph) = isFunc ? (CFGAGraph*)binaryAnalyzer.findFunc(vma) : (CFGAGraph*)binaryAnalyzer.findLoop(vma);
    }
  }
  GENERATE_SAVE_LOAD_TEMPLATE_INSTANTIATION(TCTANode)
          
  template<class Archive>
  void TCTATraceNode::serialize(Archive& ar, const unsigned int version) {
    ar & boost::serialization::base_object<TCTANode>(*this);
    ar & children;
  }
  GENERATE_SERIALIZE_TEMPLATE_INSTANTIATION(TCTATraceNode)
  
  template<class Archive>
  void TCTFunctionTraceNode::serialize(Archive& ar, const unsigned int version) {
    ar & boost::serialization::base_object<TCTATraceNode>(*this);
  }
  GENERATE_SERIALIZE_TEMPLATE_INSTANTIATION(TCTFunctionTraceNode)
  
  template<class Archive>
  void TCTRootNode::serialize(Archive& ar, const unsigned int version) {
    ar & boost::serialization::base_object<TCTATraceNode>(*this);
  }
  GENERATE_SERIALIZE_TEMPLATE_INSTANTIATION(TCTRootNode)
  
  template<class Archive>
  void TCTIterationTraceNode::serialize(Archive& ar, const unsigned int version) {
    ar & boost::serialization::base_object<TCTATraceNode>(*this);
    ar & const_cast<long&>(iterNum);
  }
  GENERATE_SERIALIZE_TEMPLATE_INSTANTIATION(TCTIterationTraceNode)
  
  template<class Archive>
  void TCTLoopNode::serialize(Archive& ar, const unsigned int version) {
    ar & boost::serialization::base_object<TCTANode>(*this);
    ar & numIteration & numAcceptedIteration;
    ar & pendingIteration & clusterNode & rejectedIterations & profileNode;
  }
  GENERATE_SERIALIZE_TEMPLATE_INSTANTIATION(TCTLoopNode)
  
  template<class Archive>
  void TCTLoopClusterNode::serialize(Archive& ar, const unsigned int version) {
    ar & boost::serialization::base_object<TCTANode>(*this);
    ar & numClusters;
    for (int i = 0; i < numClusters; i++)
      ar & clusters[i].members & clusters[i].representative;
    ar & diffRatio;
  }
  GENERATE_SERIALIZE_TEMPLATE_INSTANTIATION(TCTLoopClusterNode)
          
  template<class Archive>
  void TCTProfileNode::serialize(Archive& ar, const unsigned int version) {
    ar & boost::serialization::base_object<TCTANode>(*this);
    ar & childMap;
  }
  GENERATE_SERIALIZE_TEMPLATE_INSTANTIATION(TCTProfileNode)
          
  // ***************************************************************************
  // Implementation of serialization functions in TCT-Cluster.hpp
  // ***************************************************************************
  template<class Archive>
  void TCTClusterMemberRSD::serialize(Archive& ar, const unsigned int version) {
    ar & const_cast<int&>(nested_level);
    ar & first_id & first_rsd;
    ar & stride & length;
  }
  GENERATE_SERIALIZE_TEMPLATE_INSTANTIATION(TCTClusterMemberRSD)
  
  template<class Archive>
  void TCTClusterMembers::serialize(Archive& ar, const unsigned int version) {
    ar & max_level;
    ar & members;
  }
  GENERATE_SERIALIZE_TEMPLATE_INSTANTIATION(TCTClusterMembers)
}
