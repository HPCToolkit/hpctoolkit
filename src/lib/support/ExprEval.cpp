// (c) Peter Kankowski, 2007. http://smallcode.weblogs.us mailto:kankowski@narod.ru
// This file is a modified version from Expression Evaluator published at
//   https://www.strchr.com/expression_evaluator
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

#include "lib/support/ExprEval.hpp"

// ================================
//   Simple expression evaluator
// ================================

// Parse a number or an expression in parenthesis
double ExprEval::ParseAtom(EVAL_CHAR*& expr) 
{
    // Skip spaces
    while(*expr == ' ')
      expr++;

    // Handle the sign before parenthesis (or before number)
    bool negative = false;
    if(*expr == '-') {
      negative = true;
      expr++;
    }
    if(*expr == '+') {
      expr++;
    }

    // Check if there is parenthesis
    if(*expr == '(') {
      expr++;
      _paren_count++;
      double res = ParseSummands(expr);
      if(*expr != ')') {
        // Unmatched opening parenthesis
        _err = EEE_PARENTHESIS;
        _err_pos = expr;
        return 0;
      }
      expr++;
      _paren_count--;
      return negative ? -res : res;
    }
  
    // check if this is variable
    bool variable = _var_map->isVariable(expr);
    if (variable) {
      expr++;
    }

    // It should be a number; convert it to double
    char* end_ptr;
    double res = strtod(expr, &end_ptr);
    if(end_ptr == expr) {
      // Report error
      _err = EEE_WRONG_CHAR;
      _err_pos = expr;
      return 0;
    }

    // if the atom is a variable, substitute it 
    if (variable) {
      unsigned int index_metric = (unsigned int) res;
      double val = _var_map->getValue(index_metric);
      if (_var_map->getErrorCode() == 0) {
        res = val;
      } else {
        _err = EEE_INCORRECT_VAR;
        return 0;
      }
    }

    // Advance the pointer and return the result
    expr = end_ptr;
    return negative ? -res : res;
}

// Parse multiplication and division
double ExprEval::ParseFactors(EVAL_CHAR*& expr) 
{
    double num1 = ParseAtom(expr);
    for(;;) {
      // Skip spaces
      while(*expr == ' ')
        expr++;
      // Save the operation and position
      EVAL_CHAR op = *expr;
      EVAL_CHAR* pos = expr;
      if(op != '/' && op != '*')
        return num1;
      expr++;
      double num2 = ParseAtom(expr);
      // Perform the saved operation
      if(op == '/') {
        // Handle division by zero
        if(num2 == 0) {
          _err = EEE_DIVIDE_BY_ZERO;
          _err_pos = pos;
          return 0;
        }
        num1 /= num2;
      }
      else
        num1 *= num2;
    }
}

// Parse addition and subtraction
double ExprEval::ParseSummands(EVAL_CHAR*& expr) 
{
    double num1 = ParseFactors(expr);
    for(;;) {
      // Skip spaces
      while(*expr == ' ')
        expr++;
      EVAL_CHAR op = *expr;
      if(op != '-' && op != '+')
        return num1;
      expr++;
      double num2 = ParseFactors(expr);
      if(op == '-')
        num1 -= num2;
      else
        num1 += num2;
    }
}

double ExprEval::Eval(EVAL_CHAR* expr, BaseVarMap *var_map)
{
  _paren_count  = 0;
  _err          = EEE_NO_ERROR;
  _var_map	= var_map;

  double res    = ParseSummands(expr);

  // Now, expr should point to '\0', and _paren_count should be zero
  if(_paren_count != 0 || *expr == ')') {
    _err = EEE_PARENTHESIS;
    _err_pos = expr;
    return 0;
  }
  if(*expr != '\0') {
    _err = EEE_WRONG_CHAR;
    _err_pos = expr;
    return 0;
  }
  return res;
}

EXPR_EVAL_ERR ExprEval::GetErr() 
{
  return _err;
}

EVAL_CHAR* ExprEval::GetErrPos() 
{
  return _err_pos;
}


// =======
//  Tests
// =======

#ifdef _DEBUG
void TestExprEval() {
  ExprEval eval;
  // Some simple expressions
  assert(eval.Eval("1234") == 1234 && eval.GetErr() == EEE_NO_ERROR);
  assert(eval.Eval("1+2*3") == 7 && eval.GetErr() == EEE_NO_ERROR);

  // Parenthesis
  assert(eval.Eval("5*(4+4+1)") == 45 && eval.GetErr() == EEE_NO_ERROR);
  assert(eval.Eval("5*(2*(1+3)+1)") == 45 && eval.GetErr() == EEE_NO_ERROR);
  assert(eval.Eval("5*((1+3)*2+1)") == 45 && eval.GetErr() == EEE_NO_ERROR);

  // Spaces
  assert(eval.Eval("5 * ((1 + 3) * 2 + 1)") == 45 && eval.GetErr() == EEE_NO_ERROR);
  assert(eval.Eval("5 - 2 * ( 3 )") == -1 && eval.GetErr() == EEE_NO_ERROR);
  assert(eval.Eval("5 - 2 * ( ( 4 )  - 1 )") == -1 && eval.GetErr() == EEE_NO_ERROR);

  // Sign before parenthesis
  assert(eval.Eval("-(2+1)*4") == -12 && eval.GetErr() == EEE_NO_ERROR);
  assert(eval.Eval("-4*(2+1)") == -12 && eval.GetErr() == EEE_NO_ERROR);
  
  // Fractional numbers
  assert(eval.Eval("1.5/5") == 0.3 && eval.GetErr() == EEE_NO_ERROR);
  assert(eval.Eval("1/5e10") == 2e-11 && eval.GetErr() == EEE_NO_ERROR);
  assert(eval.Eval("(4-3)/(4*4)") == 0.0625 && eval.GetErr() == EEE_NO_ERROR);
  assert(eval.Eval("1/2/2") == 0.25 && eval.GetErr() == EEE_NO_ERROR);
  assert(eval.Eval("0.25 * .5 * 0.5") == 0.0625 && eval.GetErr() == EEE_NO_ERROR);
  assert(eval.Eval(".25 / 2 * .5") == 0.0625 && eval.GetErr() == EEE_NO_ERROR);
  
  // Repeated operators
  assert(eval.Eval("1+-2") == -1 && eval.GetErr() == EEE_NO_ERROR);
  assert(eval.Eval("--2") == 2 && eval.GetErr() == EEE_NO_ERROR);
  assert(eval.Eval("2---2") == 0 && eval.GetErr() == EEE_NO_ERROR);
  assert(eval.Eval("2-+-2") == 4 && eval.GetErr() == EEE_NO_ERROR);

  // === Errors ===
  // Parenthesis error
  eval.Eval("5*((1+3)*2+1");
  assert(eval.GetErr() == EEE_PARENTHESIS && strcmp(eval.GetErrPos(), "") == 0);
  eval.Eval("5*((1+3)*2)+1)");
  assert(eval.GetErr() == EEE_PARENTHESIS && strcmp(eval.GetErrPos(), ")") == 0);
  
  // Repeated operators (wrong)
  eval.Eval("5*/2");
  assert(eval.GetErr() == EEE_WRONG_CHAR && strcmp(eval.GetErrPos(), "/2") == 0);
  
  // Wrong position of an operator
  eval.Eval("*2");
  assert(eval.GetErr() == EEE_WRONG_CHAR && strcmp(eval.GetErrPos(), "*2") == 0);
  eval.Eval("2+");
  assert(eval.GetErr() == EEE_WRONG_CHAR && strcmp(eval.GetErrPos(), "") == 0);
  eval.Eval("2*");
  assert(eval.GetErr() == EEE_WRONG_CHAR && strcmp(eval.GetErrPos(), "") == 0);
  
  // Division by zero
  eval.Eval("2/0");
  assert(eval.GetErr() == EEE_DIVIDE_BY_ZERO && strcmp(eval.GetErrPos(), "/0") == 0);
  eval.Eval("3+1/(5-5)+4");
  assert(eval.GetErr() == EEE_DIVIDE_BY_ZERO && strcmp(eval.GetErrPos(), "/(5-5)+4") == 0);
  eval.Eval("2/"); // Erroneously detected as division by zero, but that's ok for us
  assert(eval.GetErr() == EEE_DIVIDE_BY_ZERO && strcmp(eval.GetErrPos(), "/") == 0);
  
  // Invalid characters
  eval.Eval("~5");
  assert(eval.GetErr() == EEE_WRONG_CHAR && strcmp(eval.GetErrPos(), "~5") == 0);
  eval.Eval("5x");
  assert(eval.GetErr() == EEE_WRONG_CHAR && strcmp(eval.GetErrPos(), "x") == 0);

  // Multiply errors
  eval.Eval("3+1/0+4$"); // Only one error will be detected (in this case, the last one)
  assert(eval.GetErr() == EEE_WRONG_CHAR && strcmp(eval.GetErrPos(), "$") == 0);
  eval.Eval("3+1/0+4");
  assert(eval.GetErr() == EEE_DIVIDE_BY_ZERO && strcmp(eval.GetErrPos(), "/0+4") == 0);
  eval.Eval("q+1/0)"); // ...or the first one
  assert(eval.GetErr() == EEE_WRONG_CHAR && strcmp(eval.GetErrPos(), "q+1/0)") == 0);
  eval.Eval("+1/0)");
  assert(eval.GetErr() == EEE_PARENTHESIS && strcmp(eval.GetErrPos(), ")") == 0);
  eval.Eval("+1/0");
  assert(eval.GetErr() == EEE_DIVIDE_BY_ZERO && strcmp(eval.GetErrPos(), "/0") == 0);
  
  // An emtpy string
  eval.Eval("");
  assert(eval.GetErr() == EEE_WRONG_CHAR && strcmp(eval.GetErrPos(), "") == 0);
}
#endif

// ============
// Main program
// ============
#ifdef _DEBUG

int main() {
  TestExprEval();
}
#endif
