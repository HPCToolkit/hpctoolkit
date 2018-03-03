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
 * File:   BinaryAnalyzer.cpp
 * Author: Lai Wei <lai.wei@rice.edu>
 * 
 * Created on March 1, 2018, 11:40 PM
 */

#include <unordered_map>
using std::unordered_map;

#include <unordered_set>
using std::unordered_set;

#include <CFG.h>
#include <CodeObject.h>
#include <CodeSource.h>
#include <Function.h>
#include <Symtab.h>
#include <Instruction.h>
#include <InstructionCategories.h>
#include <LineInformation.h>

using namespace Dyninst;
using namespace ParseAPI;
using namespace InstructionAPI;

#include <lib/support/diagnostics.h>

#include "BinaryAnalyzer.hpp"
#include "CFGNode.hpp"

namespace TraceAnalysis {
  class BinaryAnalyzerImpl {
  public:
    BinaryAnalyzerImpl() {}
    virtual ~BinaryAnalyzerImpl() {}

    unordered_map<VMA, CFGFunc*> CFGFuncMap;
    unordered_map<VMA, CFGLoop*> CFGLoopMap;
    
    typedef unordered_map<VMA, VMA> AddrMap;

    bool doBlock(Block* block, AddrMap& blockMap, AddrMap& raMap)
    {
      if (blockMap.find(block->start()) != blockMap.end()) return true;

      Block::Insns insns;
      block->getInsns(insns);

      for (auto iit = insns.begin(); iit != insns.end(); ++iit)
      {
        VMA addr = iit->first;
        InsnCategory cat = iit->second->getCategory();

        if (cat == c_CallInsn) {
          blockMap[block->start()] = addr;
          raMap[addr] = addr + iit->second->size();
          return true;
        }
      }

      blockMap[block->start()] = block->start();
      return false;
    }

    //----------------------------------------------------------------------
    bool processCFG(CFGAGraph* cfgGraph, vector<Block*> &blist, vector<Block*> &entryBlocks, 
          AddrMap& blockMap, unordered_set<Edge*>& backEdges) {
      // detect function calls in all basic blocks and record ones without a function call	
      vector<VMA> addrNoCall;
      AddrMap raMap;
      for (auto bit = blist.begin(); bit != blist.end(); ++bit)
        if (!doBlock(*bit, blockMap, raMap))
          addrNoCall.push_back((*bit)->start());

      // build CFG among basic blocks and sub-loops
      unordered_map<VMA, unordered_set<VMA>> CFG;

      // Add entry blocks
      for (auto bit = entryBlocks.begin(); bit != entryBlocks.end(); ++bit)
        CFG[blockMap[(*bit)->start()]] = unordered_set<VMA>();
      
      // Add all edges
      for (auto bit = blist.begin(); bit != blist.end(); ++bit) {
        Block* block = *bit;
        VMA addr = blockMap[block->start()];

        const Block::edgelist & elist = block->targets();
        for (auto eit = elist.begin(); eit != elist.end(); ++eit) {
          Edge * edge = *eit;
          // exclude any sinkEdge				
          if (!edge->sinkEdge())
            // exclude any edge that targets blocks outside of this loop
            if (blockMap.find(edge->trg()->start()) != blockMap.end())
              // exclude self edges
              if (addr != blockMap[edge->trg()->start()])
                // exclude back edges
                if (backEdges.find(edge) == backEdges.end())
                  CFG[addr].insert(blockMap[edge->trg()->start()]);
        }
      }
      
      // delete basic blocks without function calls
      for (auto ait = addrNoCall.begin(); ait != addrNoCall.end(); ++ait) {
        int addr = *ait; // = blockMap[*ait]
        for (auto mit = CFG.begin(); mit != CFG.end(); ++mit) {
          unordered_set<VMA>& dest = mit->second;
          if (dest.find(addr) != dest.end()) {
            dest.erase(addr);
            dest.insert(CFG[addr].begin(), CFG[addr].end());
            dest.erase(mit->first);
          }
        }
        CFG.erase(addr);
      }
      
      // replace address of calls with RA
      unordered_map<VMA, unordered_set<VMA>> successorMap;
      for (auto mit = CFG.begin(); mit != CFG.end(); ++mit) {
        VMA addr1 = mit->first;
        if (raMap.find(addr1) != raMap.end()) addr1 = raMap[addr1];
        // make sure that every node in CFG has an entry in successorMap
        successorMap[addr1] = unordered_set<VMA>();
        for (auto sit = (mit->second).begin(); sit != (mit->second).end(); ++sit) {
          VMA addr2 = *sit;
          if (raMap.find(addr2) != raMap.end()) addr2 = raMap[addr2];
          successorMap[addr1].insert(addr2);
          // make sure that every node in CFG has an entry in successorMap
          if (successorMap.find(addr2) == successorMap.end())
            successorMap[addr2] = unordered_set<VMA>();
        }
      }

      // Compute successor map through propagation
      unordered_map<VMA, int> outdegree;
      for (auto mit = successorMap.begin(); mit != successorMap.end(); ++mit)
        outdegree[mit->first] = mit->second.size();
      
      // Start with nodes with no out edges.
      while (outdegree.size() > 0) {
        VMA sinkAddr = 0;
        for (auto mit = outdegree.begin(); mit != outdegree.end(); ++mit)
          if (mit->second == 0) {
            sinkAddr = mit->first;
            break;
          }
        
        if (sinkAddr == 0) {
          // When every node has out edge, there must be some back edges 
          // that are not detected and removed.
          DIAG_Msg(1, "Unknown back edges detected in " << cfgGraph->toString());
          return false;
        }
        
        // Propagation
        for (auto mit = successorMap.begin(); mit != successorMap.end(); ++mit)
          if ((mit->second).find(sinkAddr) != (mit->second).end()) {
            (mit->second).insert(successorMap[sinkAddr].begin(), successorMap[sinkAddr].end());
            outdegree[mit->first] = outdegree[mit->first] - 1;
          }
        
        outdegree.erase(sinkAddr);
      }
      
      cfgGraph->setSuccessorMap(successorMap);
      
      return true;
    }

    //----------------------------------------------------------------------
    void doLoop(LoopTreeNode* ltnode, AddrMap& parentAddrMap) {
      AddrMap blockMap;

      vector<LoopTreeNode *> llist = ltnode->children;
      for (auto lit = llist.begin(); lit != llist.end(); ++lit)
        doLoop(*lit, blockMap);

      if (ltnode->loop == NULL) {
        parentAddrMap.insert(blockMap.begin(), blockMap.end());
      }
      else {
        Loop* loop = ltnode->loop;

        vector<Block*> entryBlocks;
        loop->getLoopEntries(entryBlocks);
        VMA loopAddr = (*(entryBlocks.begin()))->start();
        
        std::stringstream sstream;
        sstream << "loop_0x" << std::hex << loopAddr;
        
        CFGLoop* cfgLoop = new CFGLoop(loopAddr, sstream.str());

        vector<Block*> blist;
        loop->getLoopBasicBlocks(blist);

        vector<Edge*> backEdges;
        loop->getBackEdges(backEdges);
        unordered_set<Edge*> exEdges(backEdges.begin(), backEdges.end());

        if (processCFG(cfgLoop, blist, entryBlocks, blockMap, exEdges)) 
          CFGLoopMap[loopAddr] = cfgLoop;
        else
          delete cfgLoop;

        // update all the parentAddrMap of all blocks
        for (auto bit = blist.begin(); bit != blist.end(); ++bit)
          parentAddrMap[(*bit)->start()] = loopAddr;
      }
    }

    //----------------------------------------------------------------------

    void doFunction(Function * func)
    {
      CFGFunc* cfgFunc = new CFGFunc(func->addr(), func->name());
      
      AddrMap blockMap;
      doLoop(func->getLoopTree(), blockMap);

      const Function::blocklist & blist = func->blocks();
      vector<Block*> blockList;	
      for (auto bit = blist.begin(); bit != blist.end(); ++bit)
              blockList.push_back(*bit);

      vector<Block*> entryBlocks;
      entryBlocks.push_back(func->entry());

      unordered_set<Edge*> exEdges;
      if (processCFG(cfgFunc, blockList, entryBlocks, blockMap, exEdges))
        CFGFuncMap[cfgFunc->vma] = cfgFunc;
      else
        delete cfgFunc;
    }

    //----------------------------------------------------------------------
    bool parse(const string& filename)
    {
      SymtabAPI::Symtab* the_symtab = NULL;
      CodeObject * the_code_obj = NULL;

      //
      // open symtab, make code object and parse
      //
      if (! SymtabAPI::Symtab::openFile(the_symtab, filename)) {
        DIAG_Msg(1, "Binary file not found: " << filename);
        return false;
      }

      the_symtab->parseTypesNow();

      SymtabCodeSource * code_src = new SymtabCodeSource(the_symtab);
      the_code_obj = new CodeObject(code_src);

      the_code_obj->parse();

      //
      // iterate over parseAPI function list
      //
      const CodeObject::funclist & funcList = the_code_obj->funcs();

      for (auto fit = funcList.begin(); fit != funcList.end(); ++fit)
      {
        Function * func = *fit;
        doFunction(func);
      }

      return true;
    }
  };
  
  BinaryAnalyzer::BinaryAnalyzer() {
  }

  BinaryAnalyzer::BinaryAnalyzer(const BinaryAnalyzer& orig) {
  }

  BinaryAnalyzer::~BinaryAnalyzer() {
    for (auto it = CFGFuncMap.begin(); it != CFGFuncMap.end(); ++it) {
      std::cout << it->second->toDetailedString();
      delete it->second;
    }
    for (auto it = CFGLoopMap.begin(); it != CFGLoopMap.end(); ++it) {
      std::cout << it->second->toDetailedString();
      delete it->second;
    }
  }
  
  bool BinaryAnalyzer::parse(const string& filename) {
    BinaryAnalyzerImpl impl;
    bool ret = impl.parse(filename);
    if (ret) {
      this->CFGFuncMap.insert(impl.CFGFuncMap.begin(), impl.CFGFuncMap.end());
      this->CFGLoopMap.insert(impl.CFGLoopMap.begin(), impl.CFGLoopMap.end());
    }
    return ret;
  }
}