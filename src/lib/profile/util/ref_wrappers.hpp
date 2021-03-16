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
// Copyright ((c)) 2019-2020, Rice University
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

#ifndef HPCTOOLKIT_PROFILE_UTIL_REF_WRAPPERS_H
#define HPCTOOLKIT_PROFILE_UTIL_REF_WRAPPERS_H

#include <functional>
#include <optional>
#include <type_traits>
#include <tuple>
#include <variant>

/// \file
/// Helper header providing specialized variants of std::optional and
/// std::variant, which store references instead of the given types.
/// Unlike the STL version, the references themselves are returned insteaed of
/// requiring a constant `.get()` as for std::reference_wrapper.

namespace hpctoolkit::util {

/// Version of std::reference_wrapper that allows for hashing and comparison.
/// These additional operators act as if the type was a T*, so the actual
/// objects need not be comparable.
template<class T>
class reference_index {
private:
  std::reference_wrapper<T> d;

public:
  template<class U>
  reference_index(U&& x) noexcept : d(std::forward<U>(x)) {};
  reference_index(const std::reference_wrapper<T>& o) noexcept : d(o) {};

  reference_index(const reference_index&) noexcept = default;
  template<class U>
  reference_index(const reference_index<U>& o) noexcept : d(o.get()) {};

  reference_index& operator=(const reference_index&) noexcept = default;

  operator T&() const noexcept { return d.get(); }
  T& get() const noexcept { return d.get(); }

  T* operator->() const noexcept { return &d.get(); }

  bool operator==(const reference_index& o) const noexcept { return &get() == &o.get(); }
  bool operator!=(const reference_index& o) const noexcept { return &get() != &o.get(); }
  bool operator<(const reference_index& o) const noexcept { return &get() < &o.get(); }
  bool operator<=(const reference_index& o) const noexcept { return &get() <= &o.get(); }
  bool operator>(const reference_index& o) const noexcept { return &get() > &o.get(); }
  bool operator>=(const reference_index& o) const noexcept { return &get() >= &o.get(); }
};

/// Semi-equivalent to std::optional<T&>, without the errors.
template<class T>
class optional_ref : private std::optional<reference_index<T>> {
  using Base = std::optional<reference_index<T>>;
  friend struct std::hash<optional_ref>;

public:
  optional_ref() noexcept = default;
  optional_ref(std::nullopt_t) noexcept {};
  optional_ref(const optional_ref&) noexcept = default;
  optional_ref(optional_ref&&) noexcept = default;
  optional_ref(T& v) noexcept : Base(v) {};
  ~optional_ref() = default;

  optional_ref& operator=(std::nullopt_t n) noexcept {
    Base::operator=(n);
    return *this;
  }
  optional_ref& operator=(const optional_ref& o) noexcept {
    Base::operator=((const Base&)o);
    return *this;
  }
  optional_ref& operator=(optional_ref&& o) noexcept {
    Base::operator=((Base&&)std::move(o));
    return *this;
  }

  optional_ref& operator=(T& v) noexcept {
    Base::operator=(v);
    return *this;
  }

  constexpr T* operator->() const { return &Base::operator->()->get(); }
  constexpr T& operator*() const { return Base::operator*().get(); }

  using Base::operator bool;
  using Base::has_value;

  constexpr bool operator==(const optional_ref& o) const noexcept {
    return ((const Base&)*this) == ((const Base&)o);
  }
  constexpr bool operator!=(const optional_ref& o) const noexcept {
    return ((const Base&)*this) != ((const Base&)o);
  }
  constexpr bool operator<(const optional_ref& o) const noexcept {
    return ((const Base&)*this) < ((const Base&)o);
  }
  constexpr bool operator<=(const optional_ref& o) const noexcept {
    return ((const Base&)*this) <= ((const Base&)o);
  }
  constexpr bool operator>(const optional_ref& o) const noexcept {
    return ((const Base&)*this) > ((const Base&)o);
  }
  constexpr bool operator>=(const optional_ref& o) const noexcept {
    return ((const Base&)*this) >= ((const Base&)o);
  }

  constexpr T& value() const { return Base::value().get(); }

  template<class U>
  constexpr T& value_or(U& d) { return Base::value_or(d).get(); }

  using Base::swap;
  using Base::reset;
};

/// Marker for variant_ref to store a pair of refs (instead of a ref to a pair)
template<class X, class Y>
struct ref_pair {
  ref_pair() = delete;
  ~ref_pair() = delete;
};

namespace {
  template<class T> struct vr_ref_wrapper_s { using type = reference_index<T>; };
  template<class X, class Y>
  struct vr_ref_wrapper_s<ref_pair<X, Y>> {
    using type = std::pair<reference_index<X>, reference_index<Y>>;
  };
  template<> struct vr_ref_wrapper_s<std::monostate> { using type = std::monostate; };
  template<class T>
  using vr_ref_wrapper = typename vr_ref_wrapper_s<T>::type;

  template<class> struct is_standard_ref_s : std::true_type {};
  template<class X, class Y>
  struct is_standard_ref_s<ref_pair<X,Y>> : std::false_type {};
  template<class T>
  inline constexpr bool is_standard_ref = is_standard_ref_s<T>::value;

  template<class T, class... Ts>
  inline constexpr bool in_pack = std::disjunction_v<std::is_same<T, Ts>...>;
}

/// Semi-equivalent to std::variant<Ts&...>, without the errors.
/// Also contains an automatic conversion to std::variant<const Ts&...>.
template<class... Ts>
class variant_ref : private std::variant<vr_ref_wrapper<Ts>...> {
  using Base = std::variant<vr_ref_wrapper<Ts>...>;
  friend struct std::hash<variant_ref>;

public:
  ~variant_ref() = default;

  constexpr variant_ref() noexcept = default;
  constexpr variant_ref(const variant_ref&) = default;
  constexpr variant_ref(variant_ref&&) = default;

  template<class T, std::enable_if_t<in_pack<T,Ts...> && is_standard_ref<T>, std::nullptr_t> = nullptr>
  constexpr variant_ref(T& v)
    : Base(reference_index<T>(v)) {};
  template<class X, class Y, std::enable_if_t<in_pack<ref_pair<X,Y>,Ts...>, std::nullptr_t> = nullptr>
  constexpr variant_ref(X& x, Y& y)
    : Base(std::make_pair(reference_index<X>(x), reference_index<Y>(y))) {};
  template<class T, std::enable_if_t<in_pack<T,Ts...> && is_standard_ref<T>, std::nullptr_t> = nullptr>
  constexpr variant_ref(reference_index<T> v)
    : Base(v) {};
  template<class X, class Y, std::enable_if_t<in_pack<ref_pair<X,Y>,Ts...>, std::nullptr_t> = nullptr>
  constexpr variant_ref(std::pair<reference_index<X>, reference_index<Y>> v)
    : Base(v) {};
  template<class T, std::enable_if_t<in_pack<T,Ts...> && is_standard_ref<T>, std::nullptr_t> = nullptr>
  constexpr variant_ref(std::reference_wrapper<T> v)
    : Base(reference_index<T>(v)) {};
  template<class X, class Y, std::enable_if_t<in_pack<ref_pair<X,Y>,Ts...>, std::nullptr_t> = nullptr>
  constexpr variant_ref(std::pair<std::reference_wrapper<X>, std::reference_wrapper<Y>> v)
    : Base(std::make_pair(reference_index<X>(v.first), reference_index<Y>(v.second))) {};

  template<std::size_t I, class T, std::enable_if_t<is_standard_ref<T>, std::nullptr_t> = nullptr>
  constexpr variant_ref(std::in_place_index_t<I> i, T& v)
    : Base(i, reference_index<T>(v)) {};
  template<std::size_t I, class X, class Y>
  constexpr variant_ref(std::in_place_index_t<I> i, X& x, Y& y)
    : Base(i, std::make_pair(reference_index<X>(x), reference_index<Y>(y))) {};
  template<std::size_t I, class T>
  constexpr variant_ref(std::in_place_index_t<I> i, reference_index<T> v)
    : Base(i, v) {};
  template<std::size_t I, class X, class Y>
  constexpr variant_ref(std::in_place_index_t<I> i, std::pair<reference_index<X>, reference_index<Y>> v)
    : Base(i, v) {};

  template<std::size_t I>
  constexpr variant_ref(std::in_place_index_t<I> i) : Base(i) {};

private:
  template<class T>
  struct ref_add_const : std::enable_if_t<is_standard_ref<T>, std::add_const<T>> {};
  template<class X, class Y>
  struct ref_add_const<ref_pair<X,Y>> {
    using type = ref_pair<typename ref_add_const<X>::type, typename ref_add_const<Y>::type>;
  };
  template<class T>
  using ref_add_const_t = typename ref_add_const<T>::type;

public:
  using const_t = variant_ref<ref_add_const_t<Ts>...>;

private:
  template<class T, std::size_t I>
  T convert() const {
    throw std::bad_variant_access();
  }
  template<class T, std::size_t I, class N0, class... Ns>
  T convert() const {
    if(index() == I) return {std::in_place_index<I>, std::get<I>((const Base&)*this)};
    return convert<T, I+1, Ns...>();
  }

  template<class T, class N>
  struct const_compat_1 : std::bool_constant<
    std::is_same_v<std::remove_const_t<T>, std::remove_const_t<N>>
    && (std::is_const_v<T> ? std::is_const_v<N> : true)
    && is_standard_ref<T> && is_standard_ref<N>
  > {};
  template<class TX, class TY, class NX, class NY>
  struct const_compat_1<ref_pair<TX,TY>, ref_pair<NX,NY>>
    : std::disjunction<const_compat_1<TX,NX>, const_compat_1<TY,NY>> {};
  template<class... Ns>
  struct const_compat_n {
    static inline constexpr bool value = std::disjunction_v<const_compat_1<Ts,Ns>...>;
  };
  template<class... Ns>
  static inline constexpr bool const_compat =
    std::conditional_t<sizeof...(Ts) == sizeof...(Ns), const_compat_n<Ns...>,
                       std::false_type>::value;

public:
  template<class... Ns, std::enable_if_t<const_compat<Ns...>, std::nullptr_t> = nullptr>
  operator variant_ref<Ns...>() const {
    return convert<variant_ref<Ns...>, 0, Ts...>();
  }

  constexpr variant_ref& operator=(const variant_ref& o) {
    Base::operator=(o);
    return *this;
  }
  constexpr variant_ref& operator=(variant_ref&& o) {
    Base::operator=(std::move(o));
    return *this;
  }

  template<class T>
  constexpr variant_ref operator=(std::enable_if_t<in_pack<T, Ts...> && is_standard_ref<T>, T&> v) {
    Base::operator=(reference_index<T>(v));
    return *this;
  }
  template<class X, class Y>
  constexpr variant_ref operator=(std::enable_if_t<in_pack<ref_pair<X,Y>, Ts...>, std::pair<X&, Y&>> v) {
    Base::operator=(std::make_pair(reference_index<X>(v.first), reference_index<Y>(v.second)));
    return *this;
  }

  using Base::index;
  using Base::valueless_by_exception;
  using Base::swap;

  constexpr bool operator==(const variant_ref& o) const noexcept {
    return ((const Base&)*this) == ((const Base&)o);
  }
  constexpr bool operator!=(const variant_ref& o) const noexcept {
    return ((const Base&)*this) != ((const Base&)o);
  }
  constexpr bool operator<(const variant_ref& o) const noexcept {
    return ((const Base&)*this) < ((const Base&)o);
  }
  constexpr bool operator<=(const variant_ref& o) const noexcept {
    return ((const Base&)*this) <= ((const Base&)o);
  }
  constexpr bool operator>(const variant_ref& o) const noexcept {
    return ((const Base&)*this) > ((const Base&)o);
  }
  constexpr bool operator>=(const variant_ref& o) const noexcept {
    return ((const Base&)*this) >= ((const Base&)o);
  }

private:
  template<class V, class... Vs>
  friend constexpr decltype(auto) std::visit(V&&, Vs&&...);

  template<class T, class... Ns>
  friend constexpr bool std::holds_alternative(const variant_ref<Ns...>& v) noexcept;
  template<class T>
  constexpr bool std_holds_alternative() const noexcept {
    return std::holds_alternative<vr_ref_wrapper<T>>((const Base&)*this);
  }

  template<class X, class Y, class... Ns>
  friend constexpr bool std::holds_alternative(const variant_ref<Ns...>& v) noexcept;
  template<class X, class Y>
  constexpr bool std_holds_alternative() const noexcept {
    return std::holds_alternative<vr_ref_wrapper<ref_pair<X,Y>>>((const Base&)*this);
  }

  template<class T, class... Ns>
  friend constexpr T& std::get(const variant_ref<Ns...>& v);
  template<class T>
  constexpr std::enable_if_t<!std::is_same_v<T,std::monostate>,T&> std_get() const noexcept {
    return std::get<reference_index<T>>((const Base&)*this).get();
  }
  template<class T>
  constexpr std::enable_if_t<std::is_same_v<T,std::monostate>,T&> std_get() const noexcept {
    return std::get<T>((const Base&)*this);
  }

  template<class X, class Y, class... Ns>
  friend constexpr std::pair<X&, Y&> std::get(const variant_ref<Ns...>& v);
  template<class X, class Y>
  constexpr std::pair<X&, Y&> std_get() const noexcept {
    auto x = std::get<std::pair<reference_index<X>, reference_index<Y>>>((const Base&)*this);
    return {x.first.get(), x.second.get()};
  }

  template<class T, class... Ns>
  friend constexpr hpctoolkit::util::optional_ref<T> std::get_if(const variant_ref<Ns...>& v) noexcept;
  template<class T>
  constexpr std::enable_if_t<!std::is_same_v<T,std::monostate>,optional_ref<T>> std_get_if() const noexcept {
    if(auto pv = std::get_if<reference_index<T>>(&(const Base&)*this))
      return pv->get();
    return std::nullopt;
  }
  template<class T>
  constexpr std::enable_if_t<std::is_same_v<T,std::monostate>,optional_ref<T>> std_get_if() const noexcept {
    if(auto pv = std::get_if<T>(&(const Base&)*this))
      return *pv;
    return std::nullopt;
  }

  template<class X, class Y, class... Ns>
  friend constexpr std::optional<std::pair<hpctoolkit::util::reference_index<X>, hpctoolkit::util::reference_index<Y>>>
  std::get_if(const variant_ref<Ns...>& v) noexcept;
  template<class X, class Y>
  constexpr std::optional<std::pair<reference_index<X>, reference_index<Y>>> std_get_if() const noexcept {
    if(auto pv = std::get_if<std::pair<reference_index<X>, reference_index<Y>>>(&(const Base&)*this))
      return *pv;
    return std::nullopt;
  }
};

}

namespace std {

template<class T>
struct hash<hpctoolkit::util::reference_index<T>> : private std::hash<T*> {
  std::size_t operator()(const hpctoolkit::util::reference_index<T>& v) const noexcept {
    return std::hash<T*>::operator()(&v.get());
  }
};

template<class T>
class hash<hpctoolkit::util::optional_ref<T>> {
  hash<typename hpctoolkit::util::optional_ref<T>::Base> realhash;

public:
  constexpr std::size_t operator()(const hpctoolkit::util::optional_ref<T>& v) const noexcept {
    return realhash(v);
  }
};

template<class... Ts>
struct variant_size<hpctoolkit::util::variant_ref<Ts...>>
  : std::integral_constant<std::size_t, sizeof...(Ts)> {};

template<class X, class Y>
class hash<std::pair<hpctoolkit::util::reference_index<X>, hpctoolkit::util::reference_index<Y>>> {
  hash<hpctoolkit::util::reference_index<X>> hashx;
  hash<hpctoolkit::util::reference_index<Y>> hashy;

public:
  constexpr std::size_t operator()(const std::pair<hpctoolkit::util::reference_index<X>,
      hpctoolkit::util::reference_index<Y>>& v) const noexcept {
    return hashx(v.first) ^ hashy(v.second);
  }
};

template<class... Ts>
class hash<hpctoolkit::util::variant_ref<Ts...>> {
  hash<typename hpctoolkit::util::variant_ref<Ts...>::Base> realhash;

public:
  constexpr std::size_t operator()(const hpctoolkit::util::variant_ref<Ts...>& v) const noexcept {
    return realhash(v);
  }
};

template<class T, class... Ts>
constexpr bool holds_alternative(const hpctoolkit::util::variant_ref<Ts...>& v) noexcept {
  return v.template std_holds_alternative<T>();
}
template<class X, class Y, class... Ts>
constexpr bool holds_alternative(const hpctoolkit::util::variant_ref<Ts...>& v) noexcept {
  return v.template std_holds_alternative<X, Y>();
}

template<class T, class... Ts>
constexpr T& get(const hpctoolkit::util::variant_ref<Ts...>& v) {
  return v.template std_get<T>();
}
template<class X, class Y, class... Ts>
constexpr std::pair<X&,Y&> get(const hpctoolkit::util::variant_ref<Ts...>& v) {
  return v.template std_get<X, Y>();
}

template<class T, class... Ts>
constexpr hpctoolkit::util::optional_ref<T> get_if(const hpctoolkit::util::variant_ref<Ts...>& v) noexcept {
  return v.template std_get_if<T>();
}
template<class X, class Y, class... Ts>
constexpr std::optional<std::pair<hpctoolkit::util::reference_index<X>, hpctoolkit::util::reference_index<Y>>>
get_if(const hpctoolkit::util::variant_ref<Ts...>& v) noexcept {
  return v.template std_get_if<X, Y>();
}

}

#endif  // HPCTOOLKIT_PROFILE_UTIL_REF_WRAPPERS_H
