#ifndef _CUDA_INSTRUCTION_H_
#define _CUDA_INSTRUCTION_H_

#include <map>
#include <iostream>
#include <regex>
#include <string>
#include <set>

#include "AnalyzeInstruction.hpp"

#define INSTRUCTION_DEBUG 0

#define FORALL_INS_TYPES(macro) \
  macro(INS_TYPE_MEMORY, 0)     \
  macro(INS_TYPE_FLOAT, 1)      \
  macro(INS_TYPE_INTEGER, 2)    \
  macro(INS_TYPE_TEXTRUE, 3)    \
  macro(INS_TYPE_CONTROL, 4)    \
  macro(INS_TYPE_PREDICATE, 5)  \
  macro(INS_TYPE_MISC, 6)      

#define FORALL_INS_COUNT(macro) \
  macro(INS_TYPE_COUNT, 7)

namespace CudaParse {

// basic instruction types, can be extended
#define DECLARE_INS_TYPE(TYPE, VALUE) \
  TYPE = VALUE,

enum InstructionType {
  FORALL_INS_TYPES(DECLARE_INS_TYPE)
  FORALL_INS_COUNT(DECLARE_INS_TYPE)
};

#undef DECLARE_INS_TYPE


struct Instruction {
  unsigned int offset;
  bool dual_first;
  bool dual_second;
  // short cut
  bool is_call;
  bool is_jump;
  bool is_sync;
  std::string predicate;
  std::string opcode;
  std::string port;
  std::string target;
  std::vector<std::string> modifiers;
  std::vector<std::string> operands;
  InstructionType type;
  InstructionStat *inst_stat;

  static std::map<std::string, InstructionType> opcode_types;
  static std::set<std::string> opcode_call;
  static std::set<std::string> opcode_jump;
  static std::set<std::string> opcode_sync;

  explicit Instruction(InstructionStat *inst_stat) : inst_stat(inst_stat) {}

  // constructor for dummy instruction
  explicit Instruction(unsigned int offset) : offset(offset), dual_first(false), dual_second(false),
    is_call(false), is_jump(false), is_sync(false), opcode("NOP"), type(INS_TYPE_MISC) {
    inst_stat = new InstructionStat(this);
  }

  explicit Instruction(unsigned int offset, unsigned int pc) : offset(offset), dual_first(false), dual_second(false),
    is_call(false), is_jump(false), is_sync(false), opcode("NOP"), type(INS_TYPE_MISC) {
    inst_stat = new InstructionStat(this);
    inst_stat->pc = pc;
  }

  Instruction(std::string &inst_str) : offset(0), dual_first(false), dual_second(false),
  is_call(false), is_jump(false), is_sync(false), inst_stat(NULL) {
    if (INSTRUCTION_DEBUG) {
      std::cout << inst_str << std::endl;
    }
    // parse dual
    if (inst_str.find("{") != std::string::npos) {  // Dual first
      auto pos = inst_str.find("{");
      inst_str.replace(pos, 1, " ");
      dual_first = true;
    }
    if (inst_str.find("}") != std::string::npos) {  // Dual second
      inst_str = inst_str.substr(2);
      auto pos = inst_str.find("*/");
      if (pos != std::string::npos) {  
        inst_str.replace(pos, 2, ":");
        dual_second = true;
      }
    }

    std::istringstream iss(inst_str);
    std::string s;
    if (std::getline(iss, s, ':')) {
      if (s.find("<") != std::string::npos) {
        auto pos = s.find(">");
        this->port = s.substr(1, pos - 1);
        s = s.substr(pos + 1); 
      }
      std::stringstream ss;
      ss << std::hex << s;
      ss >> offset;
      if (std::getline(iss, s, ':')) {
        s.erase(std::remove(s.begin(), s.end(), '{'), s.end());
        s.erase(std::remove(s.begin(), s.end(), '}'), s.end());
        s.erase(std::remove(s.begin(), s.end(), ';'), s.end());
        s.erase(std::remove(s.begin(), s.end(), ','), s.end());
        s.erase(std::remove(s.begin(), s.end(), '('), s.end());
        s.erase(std::remove(s.begin(), s.end(), ')'), s.end());
        s.erase(std::remove(s.begin(), s.end(), '`'), s.end());
        std::regex e("\\\\ ");
        std::string newline("\n");
        iss = std::istringstream(std::regex_replace(s, e, newline));
        while (std::getline(iss, s)) {
          if (s == "") {
            continue;
          }
          if (opcode == "") {
            // parse opcode
            if (s.find("@") != std::string::npos) {
              predicate = s;
            } else {
              std::istringstream ops(s);
              bool first_pos = true;
              while (std::getline(ops, s, '.')) {
                if (first_pos) {
                  opcode = s;
                  // set short cut
                  if (opcode_call.find(opcode) != opcode_call.end()) {
                    this->is_call = true;
                  } else if (opcode_jump.find(opcode) != opcode_jump.end()) {
                    this->is_jump = true;
                  } else if (opcode_sync.find(opcode) != opcode_sync.end()) {
                    this->is_sync = true;
                  }

                  first_pos = false;
                } else {
                  modifiers.push_back(s);
                }
              }

              if (opcode_types.find(opcode) != opcode_types.end()) {
                type = opcode_types[opcode];
              } else {
                type = INS_TYPE_MISC;
              }

              if (INSTRUCTION_DEBUG) {
                std::cout << "opcode: " << opcode;
                for (auto &modifier : modifiers) {
                  std::cout << ", modifier: " << modifier;
                }
                std::cout << std::endl;
              }
            }
          } else {
            // parse operands
            operands.push_back(s);
            if (is_jump || is_sync) {
              auto pos = s.find(".L_");
              if (pos != std::string::npos) {
                auto end_pos = pos + 3;
                size_t len = 0;
                while (end_pos != std::string::npos) {
                  if (!std::isdigit(s[end_pos])) {
                    break;
                  }
                  ++len;
                  ++end_pos;
                }
                this->target = s.substr(pos, len + 3);
              }
            }
          }
        }
      }
    }

    inst_stat = new InstructionStat(this);
  }

  std::string to_string() const {
    std::string ret = std::to_string(offset) + ": " + predicate + " " + opcode;
    for (auto &m : modifiers) {
      ret += "." + m;
    }
    for (auto &o : operands) {
      ret += " " + o;
    }
    return ret;
  }

  ~Instruction() {
    if (inst_stat) {
      delete inst_stat;
    }
  }
};

}

#endif
