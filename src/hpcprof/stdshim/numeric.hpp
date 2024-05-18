// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_STDSHIM_NUMERIC_H
#define HPCTOOLKIT_STDSHIM_NUMERIC_H

// This file is one of multiple "stdshim" headers, which act as a seamless
// transition library across versions of the STL. Mostly all this does is
// backport features from C++17 into C++11, sometimes by using class inheritance
// tricks, and sometimes by importing implementations from Boost or ourselves.
// Also see Google's Abseil project.

// This is the shim for <numeric>.

#include "version.hpp"

#include <iterator>
#include <numeric>
#include <type_traits>

namespace hpctoolkit::stdshim {

// All implementations here are identical to the STL from GCC 9.x-11.x,
// excluding those that take ExecutionPolicies.

#if __cplusplus < 201703L
#error This header presumes (near-complete) C++17 support!
#endif

using ::std::iota;
using ::std::accumulate;
using ::std::inner_product;
using ::std::adjacent_difference;
using ::std::partial_sum;

namespace {
using std::iterator_traits;
template<class It, class Traits = ::std::iterator_traits<It>,
         class Cat = typename Traits::iterator_category>
using is_random_access_iter = ::std::is_base_of<::std::random_access_iterator_tag, Cat>;
}

template<class InIt, class T, class BinOp>
constexpr T reduce(InIt first, InIt last, T init, BinOp op) {
  using ref = typename iterator_traits<InIt>::reference;
  static_assert(::std::is_invocable_r_v<T, BinOp&, T&, ref>);
  static_assert(::std::is_invocable_r_v<T, BinOp&, ref, T&>);
  static_assert(::std::is_invocable_r_v<T, BinOp&, T&, T&>);
  static_assert(::std::is_invocable_r_v<T, BinOp&, ref, ref>);
  if constexpr (is_random_access_iter<InIt>::value) {
    while((last - first) >= 4) {
      T v1 = op(first[0], first[1]);
      T v2 = op(first[2], first[3]);
      T v3 = op(v1, v2);
      init = op(init, v3);
      first += 4;
    }
  }
  for(; first != last; ++first)
    init = op(init, *first);
  return init;
}
template<class InIt, class T>
constexpr inline T reduce(InIt first, InIt last, T init) {
  return hpctoolkit::stdshim::reduce(first, last, ::std::move(init), ::std::plus<>{});
}
template<class InIt>
constexpr inline typename iterator_traits<InIt>::value_type
reduce(InIt first, InIt last) {
  using value_type = typename iterator_traits<InIt>::value_type;
  return hpctoolkit::stdshim::reduce(first, last, value_type{}, ::std::plus<>{});
}

template<class InIt1, class InIt2, class T, class BinOp1, class BinOp2>
constexpr T transform_reduce(InIt1 first1, InIt1 last1, InIt2 first2, T init, BinOp1 op1, BinOp2 op2) {
  if constexpr (is_random_access_iter<InIt1>::value && is_random_access_iter<InIt2>::value) {
    while((last1 - first1) >= 4) {
      T v1 = op1(op2(first1[0], first2[0]), op2(first1[1], first2[1]));
      T v2 = op1(op2(first1[2], first2[2]), op2(first1[3], first2[3]));
      T v3 = op1(v1, v2);
      init = op1(init, v3);
      first1 += 4;
      first2 += 4;
    }
    for(; first1 != last1; ++first1, (void)++first2)
      init = op1(init, op2(*first1, *first2));
    return init;
  }
}
template<class InIt1, class InIt2, class T>
constexpr inline T transform_reduce(InIt1 first1, InIt1 last1, InIt2 first2, T init) {
  return hpctoolkit::stdshim::reduce(first1, last1, first2, std::move(init),
      ::std::plus<>{}, ::std::multiplies<>{});
}

template<class InIt, class T, class BinOp, class UnOp>
constexpr T transform_reduce(InIt first, InIt last, T init, BinOp op, UnOp trans) {
  if constexpr (is_random_access_iter<InIt>::value) {
    while((last - first) >= 4) {
      T v1 = op(trans(first[0]), trans(first[1]));
      T v2 = op(trans(first[2]), trans(first[3]));
      T v3 = op(v1, v2);
      init = op(init, v3);
      first += 4;
    }
  }
  for(; first != last; ++first)
    init = op(init, trans(*first));
  return init;
}

template<class InIt, class OutIt, class BinOp, class T>
constexpr OutIt inclusive_scan(InIt first, InIt last, OutIt result, BinOp op, T init) {
  for(; first != last; ++first)
    *result++ = init = op(init, *first);
  return result;
}
template<class InIt, class OutIt, class BinOp>
constexpr OutIt inclusive_scan(InIt first, InIt last, OutIt result, BinOp op) {
  if(first != last) {
    auto init = *first;
    *result++ = init;
    ++first;
    if(first != last)
      result = hpctoolkit::stdshim::inclusive_scan(first, last, result, op, ::std::move(init));
  }
  return result;
}
template<class InIt, class OutIt>
constexpr inline OutIt inclusive_scan(InIt first, InIt last, OutIt result) {
  return hpctoolkit::stdshim::inclusive_scan(first, last, result, ::std::plus<>{});
}

template<class InIt, class OutIt, class BinOp, class UnOp, class T>
constexpr OutIt transform_inclusive_scan(InIt first, InIt last, OutIt result, BinOp op, UnOp trans, T init) {
  for(; first != last; ++first)
    *result++ = init = op(init, trans(*first));
  return result;
}
template<class InIt, class OutIt, class BinOp, class UnOp>
constexpr OutIt transform_inclusive_scan(InIt first, InIt last, OutIt result, BinOp op, UnOp trans) {
  if(first != last) {
    auto init = trans(*first);
    *result++ = init;
    ++first;
    if(first != last)
      result = hpctoolkit::stdshim::transform_inclusive_scan(first, last, result, op, trans, ::std::move(init));
  }
  return result;
}

template<class InIt, class OutIt, class T, class BinOp>
constexpr OutIt exclusive_scan(InIt first, InIt last, OutIt result, T init, BinOp op) {
  while(first != last) {
    auto v = init;
    init = op(init, *first);
    ++first;
    *result++ = ::std::move(v);
  }
  return result;
}
template<class InIt, class OutIt, class T>
constexpr OutIt exclusive_scan(InIt first, InIt last, OutIt result, T init) {
  return hpctoolkit::stdshim::exclusive_scan(first, last, result, ::std::move(init), ::std::plus<>{});
}

template<class InIt, class OutIt, class T, class BinOp, class UnOp>
constexpr OutIt transform_exclusive_scan(InIt first, InIt last, OutIt result,
    T init, BinOp op, UnOp trans) {
  while(first != last) {
    auto v = init;
    init = op(init, trans(*first));
    ++first;
    *result++ = ::std::move(v);
  }
  return result;
}

}  // hpctoolkit::stdshim

#endif  // HPCTOOLKIT_STDSHIM_NUMERIC_H
