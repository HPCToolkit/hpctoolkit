// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "CudaCFG.hpp"

#include <boost/property_map/dynamic_property_map.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/adjacency_list.hpp>

#include <algorithm>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <regex>
#include <charconv>
#include <vector>
#include <sstream>

#include <CodeSource.h>
#include <CodeObject.h>
#include <CFGFactory.h>
#include <Instruction.h>
#include <CFG.h>

using namespace Dyninst;
using namespace ParseAPI;
using namespace SymtabAPI;
using namespace InstructionAPI;
using namespace gpu::CudaCFG;


CudaBlock::CudaBlock(Dyninst::ParseAPI::CodeObject* obj, CudaCodeRegion& region,
  Dyninst::Address start, std::size_t num_strides)
  : Dyninst::ParseAPI::Block(obj, &region, start, start + num_strides * region.stride(),
        num_strides > 0 ? start + (num_strides - 1) * region.stride() : start),
    m_start(start), m_end(start + num_strides * region.stride()),
    m_stride(region.stride()) {}


static unsigned char dummy_inst[32] = {0};
void CudaBlock::getInsns(Insns& insns) const {
  for (Dyninst::Address offset = m_start; offset != m_end; offset += m_stride) {
    InstructionAPI::Operation op(cuda_op_general, "", Arch_cuda);
    insns.emplace(offset, InstructionAPI::Instruction(op, m_stride, dummy_inst, Arch_cuda));
  }
}


std::vector<std::unique_ptr<CudaCodeRegion>>
CudaCodeRegion::from_symtab(std::string_view cubin, int cuda_arch,
  Dyninst::SymtabAPI::Symtab &symtab) {
  std::vector<Symbol*> syms;
  symtab.getAllDefinedSymbols(syms);
  std::unordered_map<unsigned int, std::pair<Symbol*, std::vector<std::reference_wrapper<Symbol>>>> sections;
  for (auto* sym : syms) {
    if (sym->getType() == Symbol::SymbolType::ST_FUNCTION) {
      sections[sym->getRegion()->getRegionNumber()].second.push_back(*sym);
    } else if (sym->getType() == Symbol::SymbolType::ST_SECTION) {
      assert(sections[sym->getRegion()->getRegionNumber()].first == nullptr);
      sections[sym->getRegion()->getRegionNumber()].first = sym;
    }
  }
  std::vector<std::unique_ptr<CudaCodeRegion>> result;
  result.reserve(sections.size());
  for (auto& x : sections) {
    if (!x.second.second.empty()) {
      result.emplace_back(
        std::make_unique<CudaCodeRegion>(cubin, cuda_arch, *x.second.first,
          std::move(x.second.second)));
    }
  }
  result.shrink_to_fit();
  return result;
}


CudaCodeSource::CudaCodeSource(std::string cubin, int cuda_arch, Symtab& symtab)
  : m_cubin(std::move(cubin)) {
  for (auto& region : CudaCodeRegion::from_symtab(m_cubin, cuda_arch, symtab)) {
    for (const Symbol& function : region->functions()) {
      _hints.push_back({function.getOffset(), 0, region.get(), ""});
    }
    addRegion(std::move(region).release());
  }
  assert(!regionsOverlap());
}


CudaFactory::CudaFactory() = default;


void CudaFactory::CallTarget::resolve_to(Dyninst::ParseAPI::Block* target) noexcept {
  if (resolved_target != nullptr) {
    return;  // Silently ignore. There isn't a better answer, and error handling is messy.
  }
  resolved_target = std::move(target);
  for (Callback& cb : std::move(deferred)) {
    std::move(cb)(resolved_target);
  }
}


ParseAPI::Function* CudaFactory::ensure_function(Dyninst::Address addr,
  Dyninst::ParseAPI::CodeObject* obj, Dyninst::ParseAPI::CodeRegion* region,
  Dyninst::InstructionSource* isrc) {
  std::unique_lock<std::mutex> l(functions_lock);
  auto [it, first] = functions.try_emplace(addr, nullptr);
  if (first) {
    auto function = std::make_unique<ParseAPI::Function>(addr, "", obj, region, isrc);
    it->second = function.get();
    funcs_.add(function.release());
  }
  return it->second;
}


std::optional<std::string> CudaFactory::nvdisasm(const CudaCodeRegion& region) {
  // Copy/convert to strings before the fork() so exceptions properly propagate
  auto region_str = std::to_string(region.index());
  std::string cubin_str(region.cubin());

  // Spawn a child nvdisasm to parse the binary, sending the CFG output to us through a pipe.
  int nvdisasm_fd;
  ::pid_t nvdisasm_pid;
  {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
      throw std::system_error(errno, std::system_category(), "Error while creating pipe");
    }
    switch (nvdisasm_pid = fork()) {
    case -1:
      throw std::system_error(errno, std::system_category(), "Error during fork");
    case 0:
      // Child. From here on out only C functions are used to avoid exceptions.
      close(pipefd[0]);
      if (!dup2(pipefd[1], STDOUT_FILENO)) {
        perror("Failed to set up environment for nvdisasm");
        exit(125);
      }
      execl(CUDA_NVDISASM_PATH, CUDA_NVDISASM_PATH,
        "-fun", region_str.c_str(),
        "-bbcfg", "-novliw", "-poff",
        cubin_str.c_str(),
        NULL);
      perror("Failed to execute nvdisasm");
      exit(127);
    default:
      // Parent.
      close(pipefd[1]);
      nvdisasm_fd = pipefd[0];
      break;
    }
  }

  // nvdisasm has been launched! Read all the data from the pipe into a std::string for safekeeping.
  std::string dot_data;
  {
    std::ostringstream ss;
    {
      char buf[1024];
      ssize_t bytes_read;
      while ((bytes_read = read(nvdisasm_fd, buf, sizeof(buf))) > 0) {
        ss << std::string_view(buf, bytes_read);
      }
    }
    dot_data = ss.str();
  }

  // Wait for the child to finish and report the status
  close(nvdisasm_fd);
  int status;
  if (waitpid(nvdisasm_pid, &status, 0) == -1) {
    perror("Failed to wait for nvdisasm child, fork failed?");
    return std::nullopt;
  }
  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    return dot_data;
  }
  return std::nullopt;
}


void CudaFactory::calls(Dyninst::ParseAPI::Block* source, const std::string& callee) {
  auto l = std::unique_lock(call_targets_lock);
  call_targets[callee].call_or_defer(
      [this, source](Dyninst::ParseAPI::Block* target){
        auto edge = std::make_unique<Edge>(source, target, Dyninst::ParseAPI::EdgeTypeEnum::CALL);
        edge->ignore_index();
        edge->install();
        edges_.add(edge.release());
      });
}


void CudaFactory::set_call_target(const std::string& function, Dyninst::ParseAPI::Block* target) {
  std::unique_lock<std::mutex> l(call_targets_lock);
  call_targets[function].resolve_to(target);
}


// Regex for a single line of content in a DOT Mrecord. The one submatch is the actual content.
// Some lines may be empty due to how the line splitting works and whitespace stripping.
// See https://graphviz.org/doc/info/shapes.html#record for the syntax.
static const std::regex REGEX_DOT_LINE(
  "(?:<[^>]*>)?"  // Port declarations
  "((?:[^\\\\]|\\\\.)*?)"  // Line content (normal character or \-escape), non-greedy
  "(?:[|{}]|\\\\[lrn]|$)",  // Line separator, newline \-escape or end-of-line
  std::regex::ECMAScript | std::regex_constants::multiline);

// Regex and replacement string for a backslash-escape.
static const std::regex REGEX_BS_ESC("\\\\(.)", std::regex::ECMAScript);
static const char REGEX_BS_ESC_REPL[] = "$1";

// Regex to parse a single line of the disassembly.
static const std::regex REGEX_DISASSEMBLY(
  R"(\s*([0-9a-fA-F]+):\s+)"  // Hexadecimal offset of the instruction (match 1)
  R"((?:(@\S*)\s+)?)"  // Optional instruction predicate prefix (match 2)
  R"(([A-Z]+)\S*)"  // Opcode (match 3)
  R"((?:\s+`\(([^)]+)\))?)"  // Optional call target specification (match 4)
  R"([^;]*;\s*)"  // Remaining operands and ending semicolin
  R"(|\s*\S+:\s*)"  // Alternative: label
  R"(|\s*\.\w+\s.*)"  // Alternative: assembler directive
  R"(|)",  // Alternative: empty line
  std::regex::ECMAScript);

// Set of known call-type instruction opcodes
static const std::set<std::string> OPCODES_CALL = {
  "CAL",  // Relative call (Maxwell/Pascal)
  "JCAL",  // Absolute call (Maxwell/Pascal)
  "CALL",  // Call (Volta/Turing/Ampere/Ada/Hopper)
};

void CudaFactory::Vertex::push_block(CudaFactory& fact, std::unique_ptr<CudaBlock> next) {
  if (tail != nullptr) {
    // Add the fallthrough edge from the current tail to the next Block
    auto edge = std::make_unique<Edge>(tail, next.get(), fallthrough_edge);
    edge->ignore_index();
    edge->install();
    fact.edges_.add(edge.release());

    // If the current tail has an unresolved call, we now know it must be a real call.
    if (needs_call_finalization()) {
      finalize_call(fact, true);
    }
  }

  // Push next onto the linked-list of Blocks
  if (head == nullptr) {
    head = next.get();
  }
  tail = next.get();
  fact.blocks_.add(next.release());
}

void CudaFactory::Vertex::finalize(CudaFactory& fact, Dyninst::ParseAPI::CodeObject* obj,
      CudaCodeRegion& region) {
  assert(head == nullptr && tail == nullptr);
  assert(REGEX_DOT_LINE.mark_count() == 1);
  assert(REGEX_DISASSEMBLY.mark_count() == 4);
  bool incomplete = false;
  Dyninst::Address start;
  std::size_t num_strides = 0;
  auto section_offset = region.section().getOffset();
  for (std::regex_iterator it = {disassembly.cbegin(), disassembly.cend(), REGEX_DOT_LINE},
      end = decltype(it){}; it != end; ++it) {
    // Interpret any backslash escapes. After this is done, we have the text as GraphViz renders it.
    auto line = std::regex_replace(it->str(1), REGEX_BS_ESC, REGEX_BS_ESC_REPL);

    // Parse the line as an instruction (or some other valid line of disassembly).
    std::smatch match;
    if (!std::regex_match(line, match, REGEX_DISASSEMBLY)) {
      // We failed to parse this line. There's no good option here, so pretend the line was a
      // comment and move on with the parsing. Hopefully it all works out.
      continue;
    }
    auto& match_predicate = match[2];
    auto& match_opcode = match[3];
    auto& match_target = match[4];
    if (match_opcode.length() == 0) {
      // This isn't an instruction. Move to the next line.
      continue;
    }

    // Calculate the offset of this instruction
    uint64_t offset = std::numeric_limits<uint64_t>::max();
    {
      const char* first = line.c_str() + match.position(1);
      const char* last = first + match.length(1);
      [[maybe_unused]] auto res = std::from_chars(first, last, offset, 16);
      assert(res.ec == std::errc{} && "Invalid hex offset but passed regex, something wrong with std::from_chars?");
      assert(*res.ptr == ':' && "Failed to parse entire offset, something wrong with std::from_chars?");
    }

    // Get the number of control words before this instruction
    auto ctl_words = region.control_words_before(offset);

    // nvdisasm offsets are relative to the section start, relocate them to global file offsets.
    offset += section_offset;

    // Add this instruction and control word(s) to the current in-progress Block
    if (!incomplete) {
      incomplete = true;
      start = offset - ctl_words * region.stride();
      num_strides = 0;
    }
    num_strides += ctl_words + 1;

    // Mark the control flow behavior of this instruction for when the block is created later.
    if (match_predicate.length() > 0) {
      target_edge = Dyninst::ParseAPI::EdgeTypeEnum::COND_TAKEN;
      fallthrough_edge = Dyninst::ParseAPI::EdgeTypeEnum::COND_NOT_TAKEN;
    } else {
      target_edge = Dyninst::ParseAPI::EdgeTypeEnum::DIRECT;
      fallthrough_edge = Dyninst::ParseAPI::EdgeTypeEnum::DIRECT;
    }

    if (OPCODES_CALL.count(match_opcode) > 0) {
      // The basic block ends with this instruction, create the Block early to represent this.
      auto block = std::make_unique<CudaBlock>(obj, region, start, num_strides);
      push_block(fact, std::move(block));
      incomplete = false;

      // NB: We can't create the call Edge here due to ambiguity. See #finalize_call for details.
      unresolved_call = {match_target, target_edge, fallthrough_edge};
      target_edge = Dyninst::ParseAPI::EdgeTypeEnum::CALL;
      fallthrough_edge = Dyninst::ParseAPI::EdgeTypeEnum::CALL_FT;
      continue;
    }
  }

  // If there is still an in-progress block on the floor, create it.
  if (incomplete) {
    auto block = std::make_unique<CudaBlock>(obj, region, start, num_strides);
    push_block(fact, std::move(block));
  }
}

void CudaFactory::Vertex::finalize_call(CudaFactory& fact, bool is_call) {
  assert(needs_call_finalization());
  const auto& [callee, alt_target_edge, alt_fallthrough_edge] = unresolved_call.value();
  if (is_call) {
    fact.calls(tail, callee);
  } else {
    target_edge = alt_target_edge;
    fallthrough_edge = alt_fallthrough_edge;
  }
  unresolved_call = std::nullopt;
}


// Regex to find the function symbol directive in a disassembly.
static const std::regex REGEX_TYPE_FUNCTION("\\.type\\s+([^,\\s]+)\\s*,\\s*@function", std::regex::ECMAScript);


void CudaFactory::parse_region(CudaCodeRegion& region,
  Dyninst::ParseAPI::CodeObject* obj, Dyninst::InstructionSource* isrc) {
  // Early exit if this Region is already parsed or in-progress
  {
    auto l = std::unique_lock(regions_lock);
    if (!regions.insert(&region).second) {
      return;
    }
  }

  auto dot_data = nvdisasm(region);
  if (!dot_data) {
    std::cout << "WARNING: unable to parse section: " << region.section().getMangledName() << std::endl;

    // Construct a dummy CFG with a single Block per function Symbol
    for (const Dyninst::SymtabAPI::Symbol& function : region.functions()) {
      auto* result = ensure_function(function.getOffset(), obj, &region, isrc);
      result->rename(function.getMangledName());
      auto block = std::make_unique<CudaBlock>(obj, region, function.getOffset(),
          function.getSize() / region.stride());
      result->add_block(block.get());
      result->setEntryBlock(block.get());
      set_call_target(result->name(), result->entry());
      blocks_.add(block.release());
    }
    return;
  }

  // Parse the `nvdisasm` DOT output using the parser from Boost.Graph
  boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, Vertex> graph;
  {
    boost::dynamic_properties dp(boost::ignore_other_properties);
    dp.property("label", boost::get(&Vertex::disassembly, graph));
    boost::read_graphviz(*dot_data, graph, dp);
  }

  // Parse each vertex in the DOT graph and convert it into one (or more) CudaBlocks
  std::vector<ParseAPI::Function*> new_functions;
  for (auto [vertit, vertend] =  boost::vertices(graph); vertit != vertend; ++vertit) {
    Vertex& vertex = graph[*vertit];
    vertex.finalize(*this, obj, region);

    // If this Vertex represents a function, create a corresponding Function with just the
    // entry Block. We'll fill in the rest later, but this allows creating Edges.
    if (std::smatch match; std::regex_search(vertex.disassembly, match, REGEX_TYPE_FUNCTION)) {
      auto* function = ensure_function(vertex.head->start(), obj, &region, isrc);
      function->rename(match.str(1));
      function->add_block(vertex.head);
      function->setEntryBlock(vertex.head);
      set_call_target(match.str(1), function->entry());
      new_functions.push_back(function);
    }
  }

  // Parse each edge in the DOT graph and create a corresponding Edge
  for (auto [edgeit, edgeend] = boost::edges(graph); edgeit != edgeend; ++edgeit) {
    Vertex& src = graph[boost::source(*edgeit, graph)];
    const Vertex& trg = graph[boost::target(*edgeit, graph)];
    if (!src.valid() || !trg.valid()) {
      // One of the vertices for this edge failed to parse completely. Drop the edge.
      continue;
    }

    // Finalize the outgoing control flow if it needs additional finalization
    if (src.needs_call_finalization()) {
      src.finalize_call(*this,
        boost::out_degree(boost::source(*edgeit, graph), graph) <= 1
        && src.tail->end() == trg.head->start());
    }

    // Create the Edge and install it
    auto edge = std::make_unique<Edge>(src.tail, trg.head,
        src.tail->end() == trg.head->start() ? src.fallthrough_edge : src.target_edge);
    edge->ignore_index();
    edge->install();
    edges_.add(edge.release());
  }

  // Ensure all Vertices have been fully finalized
  for (auto [vertit, vertend] =  boost::vertices(graph); vertit != vertend; ++vertit) {
    Vertex& vertex = graph[*vertit];
    if (vertex.needs_call_finalization()) {
      vertex.finalize_call(*this, true);
    }
  }

  // Assign Blocks to the Functions we created, by following the control flow in a DFS.
  for (auto* function : new_functions) {
    std::deque<ParseAPI::Block*> q({function->entry()});
    while (!q.empty()) {
      auto* source = q.back();
      q.pop_back();
      for (auto* edge : source->targets()) {
        if (edge->type() == Dyninst::ParseAPI::EdgeTypeEnum::CALL)
          continue;
        auto* target = edge->trg();
        if (!function->contains(target)) {
          function->add_block(target);
          q.push_back(target);
        }
      }
    }
  }
}


Dyninst::ParseAPI::Function* CudaFactory::mkfunc(Dyninst::Address addr,
  Dyninst::ParseAPI::FuncSource fsrc, std::string, Dyninst::ParseAPI::CodeObject* obj,
  Dyninst::ParseAPI::CodeRegion* region, Dyninst::InstructionSource* isrc) {
  assert(fsrc == Dyninst::ParseAPI::FuncSource::HINT);
  parse_region(*dynamic_cast<CudaCodeRegion*>(region), obj, isrc);
  return ensure_function(addr, obj, region, isrc);
}
