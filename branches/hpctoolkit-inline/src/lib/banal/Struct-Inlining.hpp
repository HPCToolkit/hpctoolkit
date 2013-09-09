//
//  $Id$
//

#ifndef Banal_Struct_Inlining_hpp
#define Banal_Struct_Inlining_hpp

#include <iostream>
#include <string>
#include <list>

class CallSite {
private:
  string fileName;
  unsigned int lineNumber;

public:
  CallSite(string &_fileName, unsigned int _lineNumber) :
    fileName(_fileName), lineNumber(_lineNumber){ };
  string &getFileName() { return fileName; };
  unsigned long getLineNumber() { return lineNumber; };
  void Dump() {
    std::cout << ", <filename>=" << fileName << ", <offset>=" << lineNumber << endl;
  }
};

class CallingContext: public CallSite {
private:
  string calleeName;
  CallingContext *next;

public:
  CallingContext(string &_calleeName, string&fileName, unsigned int line, CallingContext *_next) :
    CallSite(fileName, line), calleeName(_calleeName), next(_next) { };
  CallingContext *Next() { return next; };
  string &getCalleeName() { return calleeName; };
  void Dump() {
    std::cout << "<procedure name>=" << calleeName;
    CallSite::Dump();
  }
};

typedef std::list<CallingContext*> StaticCallChain;

bool openSymtab(const std::string &filename);
void closeSymtab(void);

CallingContext *analyzeInlining(unsigned long addr);

#endif // Banal_Struct_Inlining_hpp
