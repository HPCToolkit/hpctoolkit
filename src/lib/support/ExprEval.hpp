// This file is a modified version from Expression Evaluator published at
//   https://www.strchr.com/expression_evaluator

#ifndef __ExprEval_H__
#define  __ExprEval_H__

#include <lib/support/BaseVarMap.hpp>   // basic var map class

// Error codes enumeration
enum EXPR_EVAL_ERR {
  EEE_NO_ERROR = 0,
  EEE_PARENTHESIS = 1,
  EEE_WRONG_CHAR = 2,
  EEE_DIVIDE_BY_ZERO = 3,
  EEE_INCORRECT_VAR = 4
};

//typedef char EVAL_CHAR;
#define EVAL_CHAR char


// Parser class to evaluate math expression
// The math expression has to be simple operators:
// +,-,*, /, ( and ) 
// Other than those, it will emit error code
class ExprEval {
private:
  EXPR_EVAL_ERR _err;
  EVAL_CHAR* _err_pos;
  int _paren_count;

  // variable maps for value substitution
  BaseVarMap *_var_map;

  // Parse a number or an expression in parenthesis
  double ParseAtom(EVAL_CHAR*& expr) ;

  // Parse multiplication and division
  double ParseFactors(EVAL_CHAR*& expr) ;
  //
  // parse a sum or substraction
  double ParseSummands(EVAL_CHAR*& expr) ;

public:
  // main method to evaluate a math expression
  double  Eval(EVAL_CHAR* expr, BaseVarMap *var_map);

  // get the error code
  EXPR_EVAL_ERR GetErr();

  // the character position of the error
  EVAL_CHAR*    GetErrPos();
  
};

#endif
