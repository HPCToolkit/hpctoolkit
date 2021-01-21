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
// Copyright ((c)) 2020, Rice University
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

#ifndef HPCTOOLKIT_PROFILE_UTIL_RAGGED_VECTOR_H
#define HPCTOOLKIT_PROFILE_UTIL_RAGGED_VECTOR_H

#include "once.hpp"
#include "locked_unordered.hpp"
#include "log.hpp"

#include "../stdshim/shared_mutex.hpp"

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <vector>

namespace hpctoolkit {

class ProfilePipeline;

namespace util {

template<class...> class ragged_vector;
template<class...> class ragged_map;

namespace detail {
/// Structure definition for a set of ragged vectors. One of these must be
/// available to construct a ragged_vector, and stay available to ensure
/// validity.
template<class... InitArgs>
class ragged_struct {
public:
  ragged_struct() : complete(false), m_size(0), m_entries() {};
  ~ragged_struct() = default;

  using initializer_t = std::function<void(void*, InitArgs&&...)>;
  using destructor_t = std::function<void(void*)>;

private:
  struct entry {
    entry(initializer_t&& i, destructor_t&& d, std::size_t s, std::size_t o)
      : initializer(std::move(i)), destructor(std::move(d)), size(s), offset(o)
      {};
    initializer_t initializer;
    destructor_t destructor;
    std::size_t size;
    std::size_t offset;
  };

  // General member, which contains all the info that may be copied into either
  // kind of member. Automatically converts into the correct version when
  // needed.
  struct generic_member {
    generic_member(ragged_struct& b, std::size_t i, const entry& e)
      : m_base(b), m_index(i), m_entry(e) {};

    ragged_struct& m_base;
    std::size_t m_index;
    entry m_entry;
  };

  template<class T>
  class generic_typed_member : public generic_member {
  public:
    generic_typed_member(generic_member&& m) : generic_member(std::move(m)) {};
    ~generic_typed_member() = default;
  };

public:
  /// Member of a ragged_vector. These references are required for access into
  /// applicable ragged_vectors. Otherwise they're mostly opaque.
  class member {
  public:
    member() : m_base(nullptr) {};
    ~member() = default;
    member(const member&) = default;
    member(member&&) = default;

    member(generic_member&& g)
      : m_base(&g.m_base), m_index(g.m_index), m_offset(g.m_entry.offset),
        m_size(g.m_entry.size) {};

    member& operator=(const member&) = default;
    member& operator=(member&&) = default;
    member& operator=(generic_member&& g) { return operator=(member(std::move(g))); }

    std::size_t index() const noexcept { return m_index; }
    std::size_t offset() const noexcept { return m_offset; }
    std::size_t size() const noexcept { return m_size; }

  private:
    friend class ragged_struct;

    ragged_struct* m_base;
    std::size_t m_index;
    std::size_t m_offset;  // Copied out for faster accesses.
    std::size_t m_size;  // Also copied out for faster accesses.
  };

  /// Member of a ragged_map. Like member, but larger and sparser.
  class sparse_member {
  public:
    sparse_member() : m_base(nullptr) {};
    ~sparse_member() = default;
    sparse_member(const sparse_member&) = default;
    sparse_member(sparse_member&&) = default;

    sparse_member(generic_member&& g)
      : m_base(&g.m_base), m_index(g.m_index), m_size(g.m_entry.size),
        m_init(g.m_entry.initializer), m_des(g.m_entry.destructor) {};

    sparse_member& operator=(const sparse_member&) = default;
    sparse_member& operator=(sparse_member&&) = default;
    sparse_member& operator=(generic_member&& g) { return operator=(sparse_member(std::move(g))); }

    std::size_t index() const noexcept { return m_index; }
    std::size_t size() const noexcept { return m_size; }
    destructor_t destructor() const noexcept { return m_des; }

  private:
    friend class ragged_struct;

    ragged_struct* m_base;
    std::size_t m_index;
    std::size_t m_size;
    initializer_t m_init;
    destructor_t m_des;
  };

  /// Register an arbitrary block of memory for future ragged_vectors.
  /// `init` is called to initialize the block, `des` to destroy it.
  // MT: Internally Synchronized
  generic_member add(std::size_t sz, initializer_t&& init, destructor_t&& des) {
    if(complete)
      util::log::fatal() << "Cannot add entries to a frozen ragged_struct!";
    std::unique_lock<stdshim::shared_mutex> l(m_mtex);
    auto idx = m_entries.size();
    m_entries.emplace_back(std::move(init), std::move(des), sz, m_size);
    m_size += sz;
    return generic_member(*this, idx, m_entries[idx]);
  }

  /// Typed version of member, to allow for (more) compile-type type safety.
  template<class T>
  class typed_member : public member {
  public:
    typed_member() : member() {};
    typed_member(const member& m) : member(m) {};
    typed_member(member&& m) : member(std::move(m)) {};
    typed_member(generic_typed_member<T>&& g) : member(std::move(g)) {};
    typed_member& operator=(generic_typed_member<T>&& g) {
      member::operator=(std::move(g));
      return *this;
    }
    ~typed_member() = default;
  };

  /// Typed version of sparse_member, to allow for (more) compile-time type safety.
  template<class T>
  class typed_sparse_member : public sparse_member {
  public:
    typed_sparse_member() : sparse_member() {};
    typed_sparse_member(const sparse_member& m) : sparse_member(m) {};
    typed_sparse_member(sparse_member&& m) : sparse_member(std::move(m)) {};
    typed_sparse_member(generic_typed_member<T>&& g) : sparse_member(std::move(g)) {};
    typed_sparse_member& operator=(generic_typed_member<T>&& g) {
      member::operator=(std::move(g));
      return *this;
    }
    ~typed_sparse_member() = default;
  };

  /// Register a typed memory block for future ragged_vectors. The constructor
  /// and destructor of T are used for initialization and destruction.
  // MT: Internally Synchronized
  template<class T, class... Args>
  generic_typed_member<T> add(Args&&... args) {
    return add(sizeof(T), [args...](void* v, InitArgs&&... iargs){
      new(v) T(std::forward<InitArgs>(iargs)..., args...);
    }, [](void* v){
      ((T*)v)->~T();
    });
  }

  /// Register a typed memory block for future ragged_vectors. Unlike add(...),
  /// this uses the default constructor and the given function for initialization.
  // MT: Internally Synchonized
  template<class T, class... Args>
  generic_typed_member<T> add_default(std::function<void(T&, InitArgs&&...)>&& init) {
    return add(sizeof(T), [init](void* v, InitArgs&&... iargs){
      new(v) T;
      init(*(T*)v, std::forward<InitArgs>(iargs)...);
    }, [](void* v){
      ((T*)v)->~T();
    });
  }

  /// Register a typed memory block for future ragged_vectors. Unlike add(...),
  /// this uses the default constructor for initialization.
  // MT: Internally Synchonized
  template<class T, class... Args>
  generic_typed_member<T> add_default() {
    return add(sizeof(T), [](void* v, InitArgs&&... iargs){
      new(v) T;
    }, [](void* v){
      ((T*)v)->~T();
    });
  }

  /// Register a typed memory block for future ragged_vectors. Uses the
  /// constructor add() would use, but still calls a custom function after.
  // MT: Internally Synchonized
  template<class T, class... Args>
  generic_typed_member<T> add_initializer(
      std::function<void(T&, InitArgs&&...)>&& init, Args&&... args) {
    return add(sizeof(T), [init, args...](void* v, InitArgs&&... iargs){
      new(v) T(std::forward<InitArgs>(iargs)..., args...);
      init(*(T*)v, std::forward<InitArgs>(iargs)...);
    }, [](void* v){
      ((T*)v)->~T();
    });
  }

  /// Freeze additions to the struct, allowing ragged_vectors to be generated.
  // MT: Externally Synchonized
  void freeze() noexcept { complete = true; }

  /// Check whether the ragged_struct is frozen and ready for actual use.
  // MT: Externally Synchronized (after freeze())
  void valid() {
    if(!complete)
      util::log::fatal() << "Cannot use a ragged_struct before freezing!";
  }

  /// Check whether a member (or typed_member) is valid with respect to this
  /// ragged_struct. Throws if its not.
  /// NOTE: For ragged_* only.
  // MT: Safe (const)
  void valid(const member& m) const {
    if(!m.m_base)
      util::log::fatal() << "Attempt to use an empty member!";
    if(m.m_base != this)
      util::log::fatal() << "Attempt to use an incompatible member!";
  }
  void valid(const sparse_member& m) const {
    if(!m.m_base)
      util::log::fatal() << "Attempt to use an empty member!";
    if(m.m_base != this)
      util::log::fatal() << "Attempt to use an incompatible member!";
  }

  /// Initialize the given member in the given data block. Adding `direct` means
  /// the data block starts with that member (sparse).
  /// NOTE: For ragged_* only.
  // MT: Internally Synchronized
  void initialize(const sparse_member& m, void* d, InitArgs&&... args) {
    m.m_init(d, std::forward<InitArgs>(args)...);
  }
  // MT: Externally Synchronized (after freeze())
  void initialize(const member& m, void* d, InitArgs&&... args) {
    valid();
    m_entries[m.m_index].initializer((char*)d + m.m_offset, std::forward<InitArgs>(args)...);
  }

  /// Destroy the given memeber in the given data block.
  // MT: Externally Synchronized
  void destruct(const sparse_member& m, void* d) {
    m.m_des(d);
  }
  // MT: Externally Synchronized (after freeze())
  void destruct(const member& m, void* d) {
    valid();
    m_entries[m.m_index].destructor((char*)d + m.m_offset);
  }

  /// Access the entry table, with or without locking.
  // MT: Externally Synchronized (after freeze())
  const std::vector<entry>& entries() {
    valid();
    return m_entries;
  }

  /// Lock up the entry table, for sparseMemberFor.
  // MT: Internally Synchronized
  std::shared_lock<stdshim::shared_mutex> lock() {
    return std::shared_lock<stdshim::shared_mutex>(m_mtex);
  }

  /// Get a member for the given index. NOTE: For ragged_* only.
  // MT: Externally Synchronized (after freeze())
  member memberFor(std::size_t i) {
    valid();
    return generic_member(*this, i, m_entries[i]);
  }
  // MT: Externally Synchronized
  sparse_member sparseMemberFor(std::shared_lock<stdshim::shared_mutex>&, std::size_t i) {
    return generic_member(*this, i, m_entries[i]);
  }

  /// Get the final total size of the struct.
  // MT: Externally Synchronized (after freeze())
  std::size_t size() {
    valid();
    return m_size;
  }

private:
  bool complete;
  stdshim::shared_mutex m_mtex;
  std::size_t m_size;
  std::vector<entry> m_entries;
};
}

/// Ragged vectors are vectors with a runtime-defined structure. Unlike normal
/// vectors, their size can't be changed after allocation. Like normal vectors,
/// the items are allocated in one big block without any other mess.
template<class... InitArgs>
class ragged_vector {
public:
  using struct_t = detail::ragged_struct<InitArgs...>;
  using member_t = typename struct_t::member;
  template<class T>
  using typed_member_t = typename struct_t::template typed_member<T>;

  template<class... Args>
  ragged_vector(struct_t& rs, Args&&... args)
    : m_base(rs), flags(m_base.entries().size()), data(new char[m_base.size()]) {
    auto base_r = std::ref(m_base);
    auto data_r = data.get();
    init = [base_r, data_r, args...](const member_t& m){
      base_r.get().initialize(m, data_r, args...);
    };
  }

  ragged_vector(const ragged_vector& o) = delete;
  ragged_vector(ragged_vector&& o)
    : m_base(o.m_base), flags(std::move(o.flags)), init(std::move(o.init)),
      data(std::move(o.data)) {};

  template<class... Args>
  ragged_vector(ragged_vector&& o, Args&&... args)
    : m_base(o.m_base), flags(std::move(o.flags)), data(std::move(o.data)) {
    auto base_r = std::ref(m_base);
    auto data_r = data.get();
    init = [base_r, data_r, args...](const member_t& m){
      base_r.get().initialize(m, data_r, args...);
    };
  }

  ~ragged_vector() noexcept {
    if(!data) return;  // Not our problem anymore.
    const auto& es = m_base.entries();
    for(std::size_t i = 0; i < es.size(); i++) {
      if(flags[i].query()) m_base.destruct(m_base.memberFor(i), data.get());
    }
  }

  /// Eagerly initialize every entry in the vector. If this is not called,
  /// members will be initialized lazily.
  // MT: Internally Synchronized
  void initialize() noexcept {
    const auto& es = m_base.entries();
    for(std::size_t i = 0; i < es.size(); i++)
      flags[i].call_nowait(init, m_base.memberFor(i));
  }

  /// Get a pointer to an entry in the vector. Throws if the member is invalid.
  // MT: Internally Synchronized
  [[nodiscard]] void* at(const member_t& m) noexcept {
    m_base.valid(m);
    flags[m.index()].call(init, m);
    return &data[m.offset()];
  }

  /// Typed version of at(), that works with typed_members.
  // MT: Internally Synchronized
  template<class T>
  [[nodiscard]] T* at(const typed_member_t<T>& m) {
    return static_cast<T*>(at((const member_t&)m));
  }

  // operator[], for ease of use.
  // MT: Internally Synchronized
  template<class T>
  [[nodiscard]] T& operator[](const typed_member_t<T>& m) { return *at(m); }
  template<class K>
  [[nodiscard]] auto& operator[](const K& k) { return *at(k(*this)); }

  struct_t& base() { return m_base; }

private:
  struct_t& m_base;
  struct cleanup {
    void operator()(char* d) { delete[] d; }
  };
  std::vector<util::OnceFlag> flags;
  std::function<void(const member_t&)> init;
  std::unique_ptr<char[], cleanup> data;
};

/// Ragged maps are sparse variation on ragged_vectors. Unlike their vector
/// counterpart, their ragged_struct can change as often as needed without
/// conflicts.
template<class... InitArgs>
class ragged_map {
public:
  using struct_t = detail::ragged_struct<InitArgs...>;
  using member_t = typename struct_t::sparse_member;
  template<class T>
  using typed_member_t = typename struct_t::template typed_sparse_member<T>;

  template<class... Args>
  ragged_map(struct_t& rs, Args&&... args) : m_base(rs) {
    auto base_r = std::ref(m_base);
    init = [base_r, args...](const member_t& m, block& b){
      b.contents.reset(new char[m.size()]);
      base_r.get().initialize(m, b.contents.get(), args...);
      b.des = m.destructor();
    };
  }

  ragged_map(const ragged_map& o) = delete;
  ragged_map(ragged_map&& o)
    : m_base(o.m_base), init(std::move(o.init)), data(std::move(o.data)) {};

  template<class... Args>
  ragged_map(ragged_map&& o, Args&&... args)
    : ragged_map(o.m_base, std::forward<Args>(args)...) {
    data = std::move(o.data);
  }

  ~ragged_map() noexcept {
    for(const auto& e: data.iterate())
      e.second.des(e.second.contents.get());
  }

  /// Remove all data contained within this ragged_map.
  // MT: Externally Synchronized
  void clear() noexcept {
    for(const auto& e: data.iterate())
      e.second.des(e.second.contents.get());
    data.clear();
  }

  /// Get a pointer to an entry in the vector. Throws if the member is invalid.
  // MT: Internally Synchronized
  [[nodiscard]] void* at(const member_t& m) noexcept {
    m_base.valid(m);
    auto& fd = data[m.index()];
    util::call_once(fd.flag, init, m, fd);
    return fd.contents.get();
  }

  /// Get a pointer to an entry in the vector. Throws if the member is invalid.
  /// Returns nullptr if the entry is not present.
  // MT: Internally Synchronized
  [[nodiscard]] void* find(const member_t& m) const noexcept {
    m_base.valid(m);
    auto* fd = data.find(m.index());
    if(fd == nullptr) return nullptr;
    // Even if its present we need to make sure its initialized
    util::call_once(fd->flag, init, m, *fd);
    return fd->contents.get();
  }

  /// Typed version of at(), that works with typed_members.
  // MT: Internally Synchronized
  template<class T>
  [[nodiscard]] T* at(const typed_member_t<T>& m) {
    return static_cast<T*>(at((const member_t&)m));
  }

  /// Typed version of find(), that works with typed_members.
  // MT: Internally Synchronized
  template<class T>
  [[nodiscard]] T* find(const typed_member_t<T>& m) const {
    return static_cast<T*>(find((const member_t&)m));
  }

  // operator[], for ease of use.
  // MT: Internally Synchronized
  template<class T>
  [[nodiscard]] T& operator[](const typed_member_t<T>& m) { return *at(m); }
  template<class K>
  [[nodiscard]] auto& operator[](const K& k) { return *at(k(*this)); }

  struct_t& base() { return m_base; }

private:
  struct_t& m_base;
  struct cleanup {
    void operator()(char* d) { delete[] d; }
  };
  struct block {
    std::once_flag flag;
    typename struct_t::destructor_t des;
    std::unique_ptr<char[], cleanup> contents;
  };
  std::function<void(const member_t&, block&)> init;
  mutable locked_unordered_map<std::size_t, block> data;
};

}
}

#endif  // HPCTOOLKIT_PROFILE_UTIL_RAGGED_VECTOR_H
