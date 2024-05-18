// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_FORMATS_CORE_H
#define HPCTOOLKIT_PROFILE_FORMATS_CORE_H

#include "../util/file.hpp"

#include <type_traits>
#include <array>
#include <vector>
#include <cassert>
#include <deque>

namespace hpctoolkit::formats {

namespace {
template<class I>
constexpr I align(I v, std::uint8_t a) {
  return (v + a - 1) / a * a;
}
}

/// Engine that determines the final location for data blocks in the given File::Instance.
// MT: Externally Synchronized
class LayoutEngine final {
public:
  LayoutEngine(util::File::Instance& file) : file(file) {}
  ~LayoutEngine() = default;

  // Not copiable or movable
  LayoutEngine(const LayoutEngine&) = delete;
  LayoutEngine& operator=(const LayoutEngine&) = delete;
  LayoutEngine(LayoutEngine&&) = delete;
  LayoutEngine& operator=(LayoutEngine&&) = delete;

  util::File::Instance& file;

  /// Allocate the next available space of the given size and alignment, and return it's offset.
  std::uint_fast64_t allocate(std::size_t size, std::uint8_t alignment) noexcept {
    auto result = align(cursor, alignment);
    cursor = result + size;
    return result;
  }

  /// Get the total number of allocated bytes, including padding.
  std::uint_fast64_t size() const noexcept {
    return cursor;
  }

private:
  std::uint_fast64_t cursor = 0;
};

/// Description of the serialized form of some data.
template<class T> struct DataTraits;

/// Mixin for implementers of DataTraits, for data with constant size.
template<class T, std::size_t Size, std::uint8_t Align = Size>
class ConstSizeDataTraits {
public:
  static inline constexpr std::uint8_t alignment = Align;
  static inline constexpr std::size_t constant_size = Size;

protected:
  using ser_type = std::array<char, Size>;
};

namespace {
template<class, class = void>
struct is_constant_size : std::false_type {
  using serialized_type = std::vector<char>;
};
template<class T>
struct is_constant_size<T, std::void_t<decltype(T::constant_size)>> : std::true_type {
  using serialized_type = std::array<char, T::constant_size>;
};

template<class, class = void>
struct is_array : std::false_type {};
template<class T>
struct is_array<T, std::void_t<decltype(std::declval<T>().elementOffset(42))>> : std::true_type {};

template<class, class = void>
struct has_parent : std::false_type {};
template<class T>
struct has_parent<T, std::void_t<typename T::parent_type>> : std::true_type {};
}

/// Wrapper for a block of data written to a File::Instance. Knows its offset in the file.
/// Note that once the data is Written, it is considered constant for all intents and purposes.
// MT: Externally Synchronized
template<class T, class Traits = DataTraits<T>>
class Written {
public:
  Written(LayoutEngine& layout, T in_data = T(), Traits in_traits = Traits())
    : data(std::move(in_data)), traits(std::move(in_traits)) {
    typename is_constant_size<Traits>::serialized_type buf = traits.serialize((const T&)data);
    m_size = buf.size();
    offset = layout.allocate(m_size, Traits::alignment);
    layout.file.writeat(offset, buf);
  }
  ~Written() = default;

  // Not copiable or movable
  Written(const Written&) = delete;
  Written& operator=(const Written&) = delete;
  Written(Written&&) = delete;
  Written& operator=(Written&&) = delete;

  const T& operator*() const noexcept { return data; }
  const T* operator->() const noexcept { return &data; }

  auto ptr() const noexcept { return offset; }
  template<class I = std::size_t>
  std::enable_if_t<is_array<Traits>::value && std::is_integral_v<I>, std::uint_fast64_t>
  ptr(I i) const noexcept {
    return offset + traits.elementOffset(i);
  };

  auto bytesize() const noexcept { return m_size; }

private:
  std::uint_fast64_t offset;
  std::uint_fast64_t m_size;
  const T data;
protected:
  Traits traits;
};

/// Variant of #Written that works with Traits that have a parent_type.
// MT: Externally Synchronized
template<class T, class Traits = DataTraits<T>>
class SubWritten : public Written<T, Traits> {
public:
  SubWritten(LayoutEngine& layout, typename Traits::parent_type& parent, T in_data = T(), Traits in_traits = Traits())
    : Written<T, Traits>(layout, std::move(in_data), std::move(in_traits)) {
    this->traits.update_parent(parent, this->ptr(), layout.size());
  }
};

/// RAII guard similar to #Written, except that it writes on the destructor instead of the
/// constructor. This allows the data to be altered before the actual write, but makes it limited
/// to only constant-size data.
// MT: Externally Synchronized
template<class T, class Traits = DataTraits<T>>
class WriteGuard {
public:
  WriteGuard(LayoutEngine& layout, T in_data = T(), Traits in_traits = Traits())
    : layout(layout), m_size(Traits::constant_size),
      offset(layout.allocate(Traits::constant_size, Traits::alignment)),
      data(std::move(in_data)), traits(std::move(in_traits)){}
  ~WriteGuard() {
    std::array<char, Traits::constant_size> buf = traits.serialize((const T&)data);
    layout.file.writeat(offset, buf);
  }

  // Not copiable or movable
  WriteGuard(const WriteGuard&) = delete;
  WriteGuard& operator=(const WriteGuard&) = delete;
  WriteGuard(WriteGuard&&) = delete;
  WriteGuard& operator=(WriteGuard&&) = delete;

  T& operator*() noexcept { return data; }
  const T& operator*() const noexcept { return data; }
  T* operator->() noexcept { return &data; }
  const T* operator->() const noexcept { return &data; }

  auto ptr() const noexcept { return offset; }
  template<class I = std::size_t>
  std::enable_if_t<is_array<Traits>::value && std::is_integral_v<I>, std::uint_fast64_t>
  ptr(I i) const noexcept {
    return offset + traits.elementOffset(i);
  };

  auto bytesize() const noexcept { return m_size; }

protected:
  LayoutEngine& layout;

private:
  const std::uint_fast64_t m_size;
  const std::uint_fast64_t offset;
  T data;
protected:
  Traits traits;
};

/// Variant of #WriteGuard that works with Traits that have a parent_type.
// MT: Externally Synchronized
template<class T, class Traits = DataTraits<T>>
class SubWriteGuard : public WriteGuard<T, Traits> {
public:
  SubWriteGuard(LayoutEngine& layout, typename Traits::parent_type& parent, T in_data = T())
    : WriteGuard<T, Traits>(layout, std::move(in_data)), parent(parent) {}
  ~SubWriteGuard() {
    this->traits.update_parent(parent, this->ptr(), this->layout.size());
  }

private:
  typename Traits::parent_type& parent;
};


/// DataTraits for a densely-packed array of a particular type.
template<class Traits>
struct Array {
  Array(Traits et = Traits()) : element_traits(std::move(et)) {}
  ~Array() = default;

  static inline std::uint8_t alignment = Traits::alignment;
  template<class T>
  std::vector<char> serialize(const T& arr) {
    std::vector<char> buf;
    for (const auto& val: arr) {
      buf.resize(align(buf.size(), alignment), 0);
      typename is_constant_size<Traits>::serialized_type subbuf = element_traits.serialize(val);
      buf.insert(buf.end(), subbuf.begin(), subbuf.end());
    }
    return buf;
  }
  template<class I>
  std::enable_if_t<is_constant_size<Traits>::value && std::is_integral_v<I>, std::size_t>
  elementOffset(I i) const noexcept {
    return i * align(Traits::constant_size, Traits::alignment);
  }

  Traits element_traits;
};

template<class T, class A>
struct DataTraits<std::vector<T, A>> : Array<DataTraits<T>> {};

template<class T, class A>
struct DataTraits<std::deque<T, A>> : Array<DataTraits<T>> {};

/// Variant of #Array that fully supports dynamic-sized elements.
template<class Traits>
class DynamicArray {
public:
  DynamicArray(Traits et = Traits()) : element_traits(std::move(et)) {}
  ~DynamicArray() = default;

  static inline std::uint8_t alignment = Traits::alignment;
  template<class T>
  std::vector<char> serialize(const T& arr) {
    std::vector<char> buf;
    suboffsets.clear();
    for (const auto& val: arr) {
      buf.resize(align(buf.size(), alignment), 0);
      suboffsets.push_back(buf.size());
      typename is_constant_size<Traits>::serialized_type subbuf = element_traits.serialize(val);
      buf.insert(buf.end(), subbuf.begin(), subbuf.end());
    }
    return buf;
  }
  template<class I>
  std::size_t elementOffset(I i) const {
    return suboffsets[i];
  }

  Traits element_traits;

private:
  std::size_t totalSize;
  std::deque<std::size_t> suboffsets;
};

}  // namespace hpctoolkit::formats

#endif  // HPCTOOLKIT_PROFILE_FORMATS_CORE_H
