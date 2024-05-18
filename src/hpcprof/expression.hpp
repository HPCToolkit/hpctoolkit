// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_EXPRESSION_H
#define HPCTOOLKIT_PROFILE_EXPRESSION_H

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace hpctoolkit {

/// Lowest-denominator mathematical expression trees. Supports a small range of
/// operations, evaluation and standard stringification. For simplicity all
/// values are doubles, always.
class Expression final {
public:
  /// Enumeration of the available kinds of values Expressions can handle.
  enum class Kind {
    /// Constant. Value is set on creation and never changed.
    constant,

    /// External Expression. Used to share ownership of sub-Expressions.
    subexpression,

    /// Variable. Value is whatever the user determines.
    variable,

    /// First of the operations. Value is derived from the Expression arguments.
    /// Enumeration values >= this are operations.
    first_op,

    /// Sum of all the arguments.
    ///     sum(X, Y, ... Z) = X + Y + ... + Z
    op_sum = first_op,

    /// Subtraction of later arguments from the first.
    ///     sub(X, Y, ... Z) = X - Y - ... - Z
    ///     sub(X) = X
    op_sub,

    /// Unary negation of the single argument.
    ///     neg(X) = -X
    op_neg,

    /// Product of all the arguments.
    ///     prod(X, Y, ... Z) = X * Y * ... * Z
    op_prod,

    /// Division of later arguments from the first.
    ///     div(X, Y, ... Z) = X / Y / ... / Z
    ///     div(X) = X
    op_div,

    /// Exponentiation right-fold (right-associative).
    ///     pow(X, Y, ... Z) = X ^ (Y ^ (... ^ Z))
    op_pow,

    /// Square root of the single argument.
    ///     sqrt(X) = X ^ 0.5
    op_sqrt,

    /// Logarithm of the first argument using the second as the base.
    ///     Y ^ log(X, Y) == X
    op_log,

    /// Natural logarithm of the first argument.
    ///     e ^ ln(X) == X
    op_ln,

    /// Minimum of the arguments.
    op_min,

    /// Maximum of the arguments.
    op_max,

    /// Floor of the single argument. Largest integer <= X.
    op_floor,

    /// Ceiling of the single argument. Smallest integer >= X.
    op_ceil,
  };

  /// Type for the user-value used for variables. Unsigned integer large enough
  /// to store a pointer or 64 bits of raw data.
  using uservalue_t = std::conditional_t<
    std::numeric_limits<std::uintptr_t>::digits < std::numeric_limits<std::uint64_t>::digits,
    std::uint64_t, std::uintptr_t>;

  struct variable_t {};
  static constexpr variable_t variable = {};

  /// Constructor for constants.
  constexpr Expression(double v) noexcept
    : m_kind(Kind::constant), m_data(v) {};
  /// Constructor for sub-Expressions
  constexpr Expression(const Expression* e) noexcept
    : m_kind(Kind::subexpression), m_data(e) {};
  /// Constructor for variables. Defaults to the "single-variable" 0.
  constexpr Expression(variable_t, uservalue_t v = 0) noexcept
    : m_kind(Kind::variable), m_data(v) {};
  /// Constructor for operations.
  Expression(Kind, std::vector<Expression>);

  // Movable and copiable
  Expression(Expression&&) = default;
  Expression(const Expression&) = default;
  Expression& operator=(Expression&&) = default;
  Expression& operator=(const Expression&) = default;

  /// Get the Kind of this Expression.
  // MT: Safe (const)
  constexpr Kind kind() const noexcept { return m_kind; }

  /// Get the constant value of this Expression. Throws if not Kind::constant.
  // MT: Safe (const)
  double constant() const;
  /// Get the sub-Expression this Expression refers to. Throws if not Kind::subexpression.
  // MT: Safe (const)
  const Expression& subexpr() const;
  /// Get the user-value of this variable Expression. Throws if not Kind::variable.
  // MT: Safe (const)
  uservalue_t var() const;
  /// Get the arguments for this operation Expression. Throws if not an
  /// operation Kind.
  // MT: Safe (const)
  const std::vector<Expression>& op_args() const;

private:
  // Wrapper around std::invoke that no-ops if the functions is a nullptr_t
  template<class F, class... Args>
  static void invoke(F&& f, Args&&... args) {
    return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
  }
  template<class... Args>
  static void invoke(std::nullptr_t, Args&&... args) {};

public:
  /// Iterate through the Expression tree, calling one of the given functions
  /// for every visited Expression. Sub-Expression links are not traversed.
  /// `f_pre_op` and `f_post_op` are called before and after visiting an
  /// operation Expression's arguments, respectively.
  /// Any functions can be replaced with `nullptr` to skip those functions.
  // MT: Safe (const)
  template<class FC, class FE, class FV, class FO1, class FO2>
  std::enable_if_t<
    (std::is_invocable_v<FC, double> || std::is_null_pointer_v<std::remove_reference_t<FC>>)
    && (std::is_invocable_v<FE, const Expression&> || std::is_null_pointer_v<std::remove_reference_t<FE>>)
    && (std::is_invocable_v<FV, uservalue_t> || std::is_null_pointer_v<std::remove_reference_t<FV>>)
    && (std::is_invocable_v<FO1, const Expression&> || std::is_null_pointer_v<std::remove_reference_t<FO1>>)
    && (std::is_invocable_v<FO2, const Expression&> || std::is_null_pointer_v<std::remove_reference_t<FO2>>),
  void> citerate(FC&& f_constant, FE&& f_subexpression, FV&& f_variable,
                 FO1&& f_pre_op, FO2&& f_post_op) const noexcept(
      (std::is_nothrow_invocable_v<FC, double> || std::is_null_pointer_v<std::remove_reference_t<FC>>)
      && (std::is_nothrow_invocable_v<FE, const Expression&> || std::is_null_pointer_v<std::remove_reference_t<FE>>)
      && (std::is_nothrow_invocable_v<FV, uservalue_t> || std::is_null_pointer_v<std::remove_reference_t<FV>>)
      && (std::is_nothrow_invocable_v<FO1, const Expression&> || std::is_null_pointer_v<std::remove_reference_t<FO1>>)
      && (std::is_nothrow_invocable_v<FO2, const Expression&> || std::is_null_pointer_v<std::remove_reference_t<FO2>>))
  {
    switch(m_kind) {
    case Kind::constant:
      return invoke(f_constant, std::get<double>(m_data));
    case Kind::subexpression:
      return invoke(f_subexpression, *std::get<const Expression*>(m_data));
    case Kind::variable:
      return invoke(f_variable, std::get<uservalue_t>(m_data));
    default:  // All other cases are operations
      invoke(f_pre_op, *this);
      for(const Expression& se: std::get<std::vector<Expression>>(m_data)) {
        se.citerate(f_constant, f_subexpression, f_variable, f_pre_op,
                    f_post_op);
      }
      return invoke(f_post_op, *this);
    }
  }

  /// Iterate through the Expression tree, traversing sub-Expression links.
  /// Otherwise the same as citerate().
  // MT: Safe (const)
  template<class FC, class FV, class FO1, class FO2>
  std::enable_if_t<
    (std::is_invocable_v<FC, double> || std::is_null_pointer_v<std::remove_reference_t<FC>>)
    && (std::is_invocable_v<FV, uservalue_t> || std::is_null_pointer_v<std::remove_reference_t<FV>>)
    && (std::is_invocable_v<FO1, const Expression&> || std::is_null_pointer_v<std::remove_reference_t<FO1>>)
    && (std::is_invocable_v<FO2, const Expression&> || std::is_null_pointer_v<std::remove_reference_t<FO2>>),
  void> citerate_all(FC&& f_constant, FV&& f_variable, FO1&& f_pre_op,
                     FO2&& f_post_op) const noexcept(
      (std::is_nothrow_invocable_v<FC, double> || std::is_null_pointer_v<std::remove_reference_t<FC>>)
      && (std::is_nothrow_invocable_v<FV, uservalue_t> || std::is_null_pointer_v<std::remove_reference_t<FV>>)
      && (std::is_nothrow_invocable_v<FO1, const Expression&> || std::is_null_pointer_v<std::remove_reference_t<FO1>>)
      && (std::is_nothrow_invocable_v<FO2, const Expression&> || std::is_null_pointer_v<std::remove_reference_t<FO2>>))
  {
    switch(m_kind) {
    case Kind::constant:
      return invoke(f_constant, std::get<double>(m_data));
    case Kind::subexpression:
      return std::get<const Expression*>(m_data)->citerate_all(f_constant,
          f_variable, f_pre_op, f_post_op);
    case Kind::variable:
      return invoke(f_variable, std::get<uservalue_t>(m_data));
    default:  // All other cases are operations
      invoke(f_pre_op, *this);
      for(const Expression& se: std::get<std::vector<Expression>>(m_data))
        se.citerate_all(f_constant, f_variable, f_pre_op, f_post_op);
      return invoke(f_post_op, *this);
    }
  }

private:
  // Evaluate the given operation with the given arguments.
  static double evaluate(Kind, const std::vector<double>&);

public:
  /// Evaluate the Expression tree, using the given function to provide values
  /// for variables.
  // MT: Safe (const)
  template<class F>
  std::enable_if_t<std::is_invocable_r_v<double, F, uservalue_t>,
  double> evaluate(F&& f) const {
    switch(m_kind) {
    case Kind::constant: return std::get<double>(m_data);
    case Kind::subexpression:
      return std::get<const Expression*>(m_data)->evaluate((F&)f);
    case Kind::variable:
      return std::invoke(f, std::get<uservalue_t>(m_data));
    default: {  // All other cases are operations
      const auto& args = std::get<std::vector<Expression>>(m_data);
      std::vector<double> arg_vs;
      arg_vs.reserve(args.size());
      for(const Expression& a: args)
        arg_vs.push_back(a.evaluate(f));
      return evaluate(m_kind, arg_vs);
    }
    }
  }

  /// Evaluate the expression tree, which must only take a single variable.
  /// The user-value of that variable must be 0.
  // MT: Safe (const)
  double evaluate(double x) const;

private:
  template<class R, class F, class... Args>
  static std::enable_if_t<std::is_invocable_r_v<std::optional<R>, F, Args...>,
  std::optional<R>> invoke_optr(F&& f, Args&&... args) {
    return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
  }
  template<class R, class... Args>
  static std::optional<R> invoke_optr(std::nullptr_t, Args&&... args) {
    return std::nullopt;
  }

  template<class F>
  std::optional<double> partial_evaluate(F&& f) const {
    switch(m_kind) {
    case Kind::constant:
      return std::get<double>(m_data);
    case Kind::subexpression:
      return std::get<const Expression*>(m_data)->partial_evaluate(f);
    case Kind::variable:
      return invoke_optr<double>(f, std::get<uservalue_t>(m_data));
    default: {  // All other cases are operations
      const auto& args = std::get<std::vector<Expression>>(m_data);
      std::vector<double> arg_vs;
      arg_vs.reserve(args.size());
      for(const Expression& a: args) {
        std::optional<double> v = a.partial_evaluate(f);
        if(!v) return std::nullopt;
        arg_vs.push_back(*v);
      }
      return evaluate(m_kind, arg_vs);
    }
    }
  }
  template<class F>
  std::optional<double> optimize_impl(F&& f) {
    std::optional<double> value;
    switch(m_kind) {
    case Kind::constant:  // Already optimized, skip some control flow
      return std::get<double>(m_data);
    case Kind::subexpression:
      value = std::get<const Expression*>(m_data)->partial_evaluate(f);
      break;
    case Kind::variable:
      value = invoke_optr<double>(f, std::get<uservalue_t>(m_data));
      break;
    default: {  // All other cases are operations
      auto& args = std::get<std::vector<Expression>>(m_data);
      std::vector<double> arg_vs;
      args.reserve(args.size());
      for(Expression& a: args) {
        std::optional<double> v = a.optimize_impl(f);
        if(!v) return std::nullopt;
        arg_vs.push_back(*v);
      }
      value = evaluate(m_kind, arg_vs);
    }
    }
    if(value) {
      m_kind = Kind::constant;
      m_data = *value;
    }
    return value;
  }

public:
  /// Optimize the Expression tree, using the given function to provide values
  /// for variables if they can be considered constant. Does not optimize
  /// sub-Expression trees, but will cut the link if the subtree is constant.
  // MT: Safe (const)
  template<class F>
  std::enable_if_t<std::is_invocable_r_v<double, F, uservalue_t>,
  void> optimize(F&& f) {
    optimize_impl(std::forward<F>(f));
  }

  /// Optimize the Expression tree, considering all variables as... variable.
  // MT: Safe (const)
  void optimize();

private:
  // Kind of the Expression
  Kind m_kind;
  // Value or arguments for the Expression.
  std::variant<double, uservalue_t, const Expression*,
               std::vector<Expression>> m_data;
};

/// Debug printing for Expressions
std::ostream& operator<<(std::ostream&, const Expression&);

namespace literals::expression_ops {
constexpr auto sum = Expression::Kind::op_sum;
constexpr auto sub = Expression::Kind::op_sub;
constexpr auto neg = Expression::Kind::op_neg;
constexpr auto prod = Expression::Kind::op_prod;
constexpr auto div = Expression::Kind::op_div;
constexpr auto pow = Expression::Kind::op_pow;
constexpr auto sqrt = Expression::Kind::op_sqrt;
constexpr auto log = Expression::Kind::op_log;
constexpr auto ln = Expression::Kind::op_ln;
constexpr auto min = Expression::Kind::op_min;
constexpr auto max = Expression::Kind::op_max;
constexpr auto floor = Expression::Kind::op_floor;
constexpr auto ceil = Expression::Kind::op_ceil;
}

}  // namespace hpctoolkit

#endif  // HPCTOOLKIT_PROFILE_EXPRESSION_H
