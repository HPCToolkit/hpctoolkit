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

#include "expression.hpp"

#include <cassert>
#include <cmath>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace hpctoolkit;

double Expression::constant() const {
  assert(m_kind == Kind::constant && "Attempt to get the constant value of a non-constant Expression");
  return std::get<double>(m_data);
}
const Expression& Expression::subexpr() const {
  assert(m_kind == Kind::subexpression && "Attempt to get the sub-Expression of an invalid Expression");
  return *std::get<const Expression*>(m_data);
}
Expression::uservalue_t Expression::var() const {
  assert(m_kind == Kind::variable && "Attempt to get the user-value of a non-variable Expression");
  return std::get<uservalue_t>(m_data);
}
const std::vector<Expression>& Expression::op_args() const {
  assert(m_kind >= Kind::first_op && "Attempt to get the arguments of a non-operation Expression");
  return std::get<std::vector<Expression>>(m_data);
}

void Expression::optimize() {
  optimize_impl(nullptr);
}

double Expression::evaluate(double x) const {
  return evaluate([x](uservalue_t v){
    assert(v == 0);
    return x;
  });
}

Expression::Expression(Kind kind, std::vector<Expression> in_args)
  : m_kind(kind), m_data(std::move(in_args)) {
  const auto& args = std::get<std::vector<Expression>>(m_data);
  switch(m_kind) {
  case Kind::constant:
  case Kind::subexpression:
  case Kind::variable:
    assert(false && "Attempt to construct a non-operation Expression with arguments!");
    std::abort();
  case Kind::op_sum:
  case Kind::op_sub:
  case Kind::op_prod:
  case Kind::op_div:
  case Kind::op_pow:
  case Kind::op_min:
  case Kind::op_max:
    assert(args.size() > 0 && "Attempt to construct an Expression with an incorrect argument count!");
    break;
  case Kind::op_neg:
  case Kind::op_sqrt:
  case Kind::op_ln:
  case Kind::op_floor:
  case Kind::op_ceil:
    assert(args.size() == 1 && "Attempt to construct an Expression with an incorrect argument count!");
    break;
  case Kind::op_log:
    assert(args.size() == 2 && "Attempt to construct an Expression with an incorrect argument count!");
    break;
  }
}

double Expression::evaluate(Kind op, const std::vector<double>& args) {
  switch(op) {
  case Kind::constant:
  case Kind::subexpression:
  case Kind::variable:
    std::abort();
  case Kind::op_sum:
    return std::accumulate(args.begin(), args.end(), (double)0.);
  case Kind::op_sub:
    return std::accumulate(args.begin(), args.end(), (double)0.,
                           [](double l, double r){ return l - r; });
  case Kind::op_neg:
    assert(args.size() == 1);
    return -args[0];
  case Kind::op_prod:
    return std::accumulate(args.begin(), args.end(), (double)0.,
                           [](double l, double r){ return l * r; });
  case Kind::op_div:
    return std::accumulate(args.begin(), args.end(), (double)0.,
                           [](double l, double r){ return l / r; });
  case Kind::op_pow:
    return std::accumulate(args.rbegin(), args.rend(), (double)0.,
                           [](double r, double l){ return std::pow(l, r); });
  case Kind::op_sqrt:
    assert(args.size() == 1);
    return std::sqrt(args[0]);
  case Kind::op_log:
    assert(args.size() == 2);
    return std::log(args[0]) / std::log(args[1]);
  case Kind::op_ln:
    assert(args.size() == 1);
    return std::log(args[0]);
  case Kind::op_min:
    return std::accumulate(args.begin(), args.end(), (double)0.,
        [](double l, double r){ return std::min<double>(l, r); });
  case Kind::op_max:
    return std::accumulate(args.begin(), args.end(), (double)0.,
        [](double l, double r){ return std::max<double>(l, r); });
  case Kind::op_floor:
    assert(args.size() == 1);
    return std::floor(args[0]);
  case Kind::op_ceil:
    assert(args.size() == 1);
    return std::ceil(args[0]);
  }
  assert(false && "Invalid Kind passed to Expression::evaluate!");
  std::abort();
}

static std::ostream& dump(std::ostream& os, const Expression& e,
                          unsigned int precedence) {
  const auto dump_infix = [&os, precedence](const std::vector<Expression>& es,
      unsigned int p, std::string_view infix) -> std::ostream& {
    if(precedence > p) os << '(';
    bool first = true;
    for(const Expression& e: es) {
      if(!first) os << infix;
      first = false;
      dump(os, e, p);
    }
    if(precedence > p) os << ')';
    return os;
  };

  switch(e.kind()) {
  case Expression::Kind::constant: return os << e.constant();
  case Expression::Kind::subexpression:
    return dump(os << "->[", e.subexpr(), 0) << "]";
  case Expression::Kind::variable: {
    std::ostringstream ss;
    ss << std::hex << e.var();
    return os << "[0x" << ss.str() << "]";
  }

  // These operators have clean infix representations
  case Expression::Kind::op_sum:  return dump_infix(e.op_args(), 1, " + ");
  case Expression::Kind::op_neg:  return dump(os << '-', e.op_args()[0], 100);
  case Expression::Kind::op_sub:  return dump_infix(e.op_args(), 1, " - ");
  case Expression::Kind::op_prod: return dump_infix(e.op_args(), 2, " * ");
  case Expression::Kind::op_div:  return dump_infix(e.op_args(), 2, " / ");
  case Expression::Kind::op_pow:  return dump_infix(e.op_args(), 3, " ^ ");

  // All other operators use a simple f(x)-style syntax
  case Expression::Kind::op_sqrt:  os << "sqrt"; break;
  case Expression::Kind::op_log:   os << "log"; break;
  case Expression::Kind::op_ln:    os << "ln"; break;
  case Expression::Kind::op_min:   os << "min"; break;
  case Expression::Kind::op_max:   os << "max"; break;
  case Expression::Kind::op_floor: os << "floor"; break;
  case Expression::Kind::op_ceil:  os << "ceil"; break;
  }
  return dump_infix(e.op_args(), 0, ", ");
}
std::ostream& hpctoolkit::operator<<(std::ostream& os, const Expression& e) {
  return dump(os, e, 0);
}
