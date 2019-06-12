#ifndef _INSTRUCTION_H_
#define _INSTRUCTION_H_

#include <string>
#include <regex>
#include <iostream>

#define INSTRUCTION_DEBUG 1

namespace CudaParse {

// basic instruction types, can be extended
enum InstructionTypes {
  INS_TYPE_MEMORY = 0,
  INS_TYPE_FLOAT = 1,
  INS_TYPE_INTEGER = 2,
  INS_TYPE_SPECIAL = 3,
  INS_TYPE_TEX = 4,
  INS_TYPE_CONTROL = 5,
  INS_TYPE_OTHER = 6,
  INS_TYPE_COUNT = 7
};


struct Instruction {
  int offset;
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
  InstructionTypes instruction_type;

  // constructor for dummy instruction
  explicit Instruction(int offset) : offset(offset), dual_first(false), dual_second(false),
  is_call(false), is_jump(false), is_sync(false) {}

  Instruction(std::string &inst_str) : offset(0), dual_first(false), dual_second(false),
  is_call(false), is_jump(false), is_sync(false) {
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
                  if (opcode == "CALL" || opcode == "CAL") { // sm_60
                    this->is_call = true;
                  } else if (opcode == "BRA" || opcode == "BRX" ||
                    opcode == "JMP" || opcode == "JMX" ||
                    opcode == "BREAK" || opcode == "JMXU") {
                    this->is_jump = true;
                  } else if (opcode == "SYNC" || opcode == "BSYNC") { 
                    // avoid Barrier Set Convergence Synchronization Point
                    //opcode.find("SSY") != std::string::npos ||
                    //opcode.find("BSSY") != std::string::npos)
                    // TODO(Keren): add more sync instructions
                    this->is_sync = true;
                  }
                  first_pos = false;
                } else {
                  modifiers.push_back(s);
                }
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
  }
};

}

#endif
