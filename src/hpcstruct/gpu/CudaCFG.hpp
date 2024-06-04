// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************

#ifndef HPCSTRUCT_GPU_CUDACFG_HPP
#define HPCSTRUCT_GPU_CUDACFG_HPP

#include <string>
#include <CodeSource.h>
#include <CodeObject.h>
#include <type_traits>
#include <optional>

namespace gpu::CudaCFG {


/// ELF architecture codes for important CUDA compute capabilities
enum {
  COMPUTE_CAPABILITY_70 = 70,  // Compute capability 7.0 (Volta)
};


/// ELF .text section in a CUDA binary.
class CudaCodeRegion : public Dyninst::ParseAPI::CodeRegion {
public:
  /// Create a new Region from the binary (path) and symbol entries.
  ///
  /// The resulting Region covers the ELF section indicated by the `section` symbol in the given
  /// `cubin` (file path). The `functions` given are symbols contained within the ELF section.
  /// `cubin` will be passed to `nvdisasm` for disassembly.
  CudaCodeRegion(std::string_view cubin, int cuda_arch, Dyninst::SymtabAPI::Symbol& section,
    std::vector<std::reference_wrapper<Dyninst::SymtabAPI::Symbol>> functions)
    : m_cubin(cubin), m_cuda_arch(cuda_arch), m_section(section), m_functions(std::move(functions))
    {}

  ~CudaCodeRegion() = default;

  /// Create a Region for every ELF section in the given `symtab`.
  ///
  /// The `symtab` is expected to have been parsed from the CUDA binary `cubin` (file path).
  /// `cubin` will be passed to `nvdisasm` for disassembly.
  static std::vector<std::unique_ptr<CudaCodeRegion>> from_symtab(std::string_view cubin,
    int cuda_arch, Dyninst::SymtabAPI::Symtab& symtab);

  CudaCodeRegion(CudaCodeRegion&&) = default;
  CudaCodeRegion& operator=(CudaCodeRegion&&) = default;
  CudaCodeRegion(const CudaCodeRegion&) = default;
  CudaCodeRegion& operator=(const CudaCodeRegion&) = default;

  /// Return the path to the CUDA binary this Region is from.
  std::string_view cubin() const { return m_cubin; }

  /// Return the machine word size for this Region.
  constexpr unsigned int stride() const {
    return m_cuda_arch >= COMPUTE_CAPABILITY_70 ? 16 : 8;
  }

  /// Return the number of control words before the given instruction
  constexpr unsigned int control_words_before(Dyninst::Address addr) const {
    if (m_cuda_arch >= COMPUTE_CAPABILITY_70) {
      // >= Volta don't have control words
      return 0;
    }
    // Before Volta, 1st of every 4 words is a control word. So if this is the 2nd of 4, it has
    // exactly one control word before it.
    return (addr / stride()) % 4 == 1 ? 1 : 0;
  }

  /// Return the ELF section Symbol for this Region.
  Dyninst::SymtabAPI::Symbol& section() const { return m_section; }

  /// Return the list of ELF function Symbols for this Region.
  const std::vector<std::reference_wrapper<Dyninst::SymtabAPI::Symbol>>& functions() const {
    return m_functions;
  }

  /// Return the index of this Region's ELF section in the ELF symbol table.
  ///
  /// This number will be passed to `nvdisasm -fun` to select this ELF section for disassembly.
  unsigned int index() const { return section().getIndex(); }

  Dyninst::Address low() const override { return section().getOffset(); }

  Dyninst::Address high() const override {
    return low() + section().getRegion()->getMemSize();
  }

  bool wasUserAdded() const override { return true; }

  /// @{
  /// These functions are required by Dyninst::ParseAPI to allow for decoding instructions, but we
  /// don't wish for the instructions in this CodeRegion to be parsed. These constants prevent
  /// ParseAPI from attempting to parse anything itself.
  bool isValidAddress(const Dyninst::Address) const override { return false; }
  void* getPtrToInstruction(const Dyninst::Address) const override { return nullptr; }
  void* getPtrToData(const Dyninst::Address) const override { return nullptr; }
  unsigned int getAddressWidth() const override { return 0; }
  bool isCode(const Dyninst::Address) const override { return false; }
  bool isData(const Dyninst::Address) const override { return false; }
  bool isReadOnly(const Dyninst::Address) const override { return false; }
  Dyninst::Address offset() const override { return 0; }
  Dyninst::Address length() const override { return 0; }
  Dyninst::Architecture getArch() const override { return Dyninst::Arch_cuda; }
  /// @}

private:
  std::string_view m_cubin;
  int m_cuda_arch;
  std::reference_wrapper<Dyninst::SymtabAPI::Symbol> m_section;
  std::vector<std::reference_wrapper<Dyninst::SymtabAPI::Symbol>> m_functions;
};


/// CUDA binary represented in Dyninst::ParseAPI.
class CudaCodeSource : public Dyninst::ParseAPI::CodeSource {
public:
  /// Create a Source for a CUDA binary.
  ///
  /// The `symtab` is to have been previously parsed from the given CUDA binary `cubin` (file path).
  /// `cubin` will be passed to `nvdisasm` for disassembly.
  CudaCodeSource(std::string cubin, int cuda_arch, Dyninst::SymtabAPI::Symtab& symtab);

  ~CudaCodeSource() = default;

  CudaCodeSource(CudaCodeSource&&) = delete;
  CudaCodeSource& operator=(CudaCodeSource&&) = delete;
  CudaCodeSource(const CudaCodeSource&) = delete;
  CudaCodeSource& operator=(const CudaCodeSource&) = delete;

  /// @{
  /// These functions are required by Dyninst::ParseAPI to allow for decoding instructions, but we
  /// don't wish for the instructions in this CodeRegion to be parsed. These constants prevent
  /// ParseAPI from attempting to parse anything itself.
  bool isValidAddress(const Dyninst::Address) const override { return false; }
  void* getPtrToInstruction(const Dyninst::Address) const override { return nullptr; }
  void* getPtrToData(const Dyninst::Address) const override { return nullptr; }
  unsigned int getAddressWidth() const override { return 0; }
  bool isCode(const Dyninst::Address) const override { return false; }
  bool isData(const Dyninst::Address) const override { return false; }
  bool isReadOnly(const Dyninst::Address) const override { return false; }
  Dyninst::Address offset() const override { return 0; }
  Dyninst::Address length() const override { return 0; }
  Dyninst::Architecture getArch() const override { return Dyninst::Arch_cuda; }
  /// @}

private:
  std::string m_cubin;
  std::vector<CudaCodeRegion> m_regions;
};


/// Basic block in a CFG for a range of CUDA instructions.
class CudaBlock : public Dyninst::ParseAPI::Block {
public:
  /// Create a Block for a range of machine words.
  ///
  /// The basic block created covers `num_strides` machine words starting from `start`.
  CudaBlock(Dyninst::ParseAPI::CodeObject* obj, CudaCodeRegion& region,
    Dyninst::Address start, std::size_t num_strides);

  ~CudaBlock() = default;

  virtual void getInsns(Insns &insns) const override;

private:
  Dyninst::Address m_start;
  Dyninst::Address m_end;
  Dyninst::Address m_stride;
};


/// CFG parser for CUDA binaries.
///
/// Although the Dyninst::ParseAPI interface is generic, this Factory only works in combination with
/// a ::CudaCodeSource above.
class CudaFactory : public Dyninst::ParseAPI::CFGFactory {
public:
  CudaFactory();

  Dyninst::ParseAPI::Function* mkfunc(Dyninst::Address, Dyninst::ParseAPI::FuncSource,
    std::string, Dyninst::ParseAPI::CodeObject*, Dyninst::ParseAPI::CodeRegion*,
    Dyninst::InstructionSource*) override;

private:
  /// Ensure a Function for the given `addr` is available and return it.
  ///
  /// This (intentionally) returns before the Function is completely filled in. Regardless of who
  /// creates the original Function, it is expected some call to #parse_region() will create the
  /// Blocks and Edges for it.
  Dyninst::ParseAPI::Function* ensure_function(Dyninst::Address addr,
      Dyninst::ParseAPI::CodeObject* obj, Dyninst::ParseAPI::CodeRegion* region,
      Dyninst::InstructionSource* isrc);

  std::unordered_map<Dyninst::Address, Dyninst::ParseAPI::Function*>
      functions; ///< Data for #ensure_function
  std::mutex functions_lock;  ///< Guard lock for #functions

  /// Run `nvdisasm` on the given `region` and (if successful) return the DOT output as a string.
  ///
  /// If `nvdisasm` fails this returns std::nullopt.
  std::optional<std::string> nvdisasm(const CudaCodeRegion& region);

  /// Parse the given `region`, or return immediately if parsing is complete or in-progress.
  ///
  /// This produces Blocks and Edges for all Functions contained within the `region`.
  void parse_region(CudaCodeRegion& region, Dyninst::ParseAPI::CodeObject* obj,
      Dyninst::InstructionSource* isrc);

  std::unordered_set<Dyninst::ParseAPI::CodeRegion*> regions;  ///< Regions parsed or in-progress
  std::mutex regions_lock;  ///< Guard lock for #regions

  /// Record that the given `source` Block calls a function symbol `callee`.
  ///
  /// Note that the function may not be parsed, if so this defers the creation of the
  /// call Edge until an entry Block for the `callee` is available.
  void calls(Dyninst::ParseAPI::Block* source, const std::string& callee);

  /// Record that the given `target` Block is the target of calls to `function`.
  void set_call_target(const std::string& function, Dyninst::ParseAPI::Block* target);


  /// Value for #call_targets, functionally a Dyninst::ParseAPI::Block* that allows enqueueing
  /// closures (std::function) to be run when the value is no longer #nullptr.
  class CallTarget {
  public:
    CallTarget() = default;
    ~CallTarget() = default;

    CallTarget(CallTarget&&) = delete;
    CallTarget& operator=(CallTarget&&) = delete;
    CallTarget(const CallTarget&) = delete;
    CallTarget& operator=(const CallTarget&) = delete;

    using Callback = std::function<void(Dyninst::ParseAPI::Block*)>;

    /// Call the given `callback` with the #resolved_target, or defer it until #resolve_to is called.
    template<class F>
    std::enable_if_t<std::is_invocable_r_v<void, F, Dyninst::ParseAPI::Block*>>
    call_or_defer(F&& callback) noexcept {
      if (resolved_target) {
        return callback((Dyninst::ParseAPI::Block*)resolved_target);
      }
      deferred.emplace_back(std::move(callback));
    }

    /// Record this given `target` as the final #resolved_target. Calls all enqueued #Callback.
    void resolve_to(Dyninst::ParseAPI::Block* target) noexcept;

  private:
    std::vector<Callback> deferred;  ///< Deferred #Callback to be called during #resolve_to.
    Dyninst::ParseAPI::Block* resolved_target = nullptr;  ///< Final value passed to #resolve_to.
  };


  std::unordered_map<std::string, CallTarget>
      call_targets;  ///< Deferred mapping of function symbols to Blocks. See #calls.
  std::mutex call_targets_lock;  ///< Guard lock for #call_targets


  /// Internal representation of a GraphViz "node" in the `nvdisasm` DOT graph output.
  class Vertex {
  public:
    /// The raw disassembly pulled from the `nvdisasm` output (a GraphViz "node" label).
    std::string disassembly;

    /// The head and tail of a singly-linked-list of Blocks for this Vertex.
    ///
    /// Although `nvdisasm -bbcfg` generates a node for every basic block in the disassembled
    /// Region, there are some cases where `nvdisasm` is missing control flow (i.e. certain call
    /// instructions) and doesn't break the basic block. During #finalize we break the Vertex into
    /// Blocks based on our own heuristics with internal fallthrough Edges.
    /// @{
    CudaBlock* head = nullptr;
    CudaBlock* tail = nullptr;
    /// @}

    /// Type of control flow outgoing from this Vertex, for non-fallthrough edges.
    Dyninst::ParseAPI::EdgeTypeEnum target_edge = Dyninst::ParseAPI::EdgeTypeEnum::DIRECT;

    /// Type of control flow outgoing from this Vertex, for fallthrough edges.
    Dyninst::ParseAPI::EdgeTypeEnum fallthrough_edge = Dyninst::ParseAPI::EdgeTypeEnum::DIRECT;

    /// Parse the #disassembly and create the Blocks this Vertex represents.
    void finalize(CudaFactory& factory, Dyninst::ParseAPI::CodeObject* obj,
      CudaCodeRegion& region);

    /// Returns true if this is a finalized Vertex.
    bool valid() const {
      return head != nullptr && tail != nullptr;
    }

    /// Returns true if this Vertex needs #finalize_call called.
    bool needs_call_finalization() const {
      return unresolved_call.has_value();
    }

    /// Finalize an unresolved outgoing call.
    ///
    /// There are some cases where call-type instructions are used as a kind of local jump, so
    /// unlike CPU architectures we can't claim it's a "real" call just from the instruction opcode.
    /// If #needs_call_finalization returns true, the caller must decide based on the full set of
    /// outgoing edges and report the final decision by calling this method.
    void finalize_call(CudaFactory& factory, bool is_call);

  private:
    std::optional<std::tuple<std::string, Dyninst::ParseAPI::EdgeTypeEnum,
        Dyninst::ParseAPI::EdgeTypeEnum>> unresolved_call;  ///< See #finalize_call

    /// Push a new Block into the linked list, #head and #tail.
    void push_block(CudaFactory& factory, std::unique_ptr<CudaBlock> next);
  };
};

} // namespace gpu::Cuda

#endif  // HPCSTRUCT_GPU_CUDACFG_HPP
