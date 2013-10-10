//
//  $Id$
//

#include <vector>

#include <Symtab.h>
#include <Function.h>
#include <Struct-Inlining.hpp>

using namespace Dyninst;
using namespace SymtabAPI;
using namespace std;


// FIXME: this uses a single static buffer for the symtab info and
// thus only works with one file at a time.
static Symtab *the_symtab = NULL;

bool
openSymtab(const std::string &filename)
{
  bool ret = Symtab::openFile(the_symtab, filename);
  if (! ret) {
    return false;
  }

  the_symtab->parseTypesNow();
  return true;
}


void
closeSymtab(void)
{
  // FIXME: Matt's dyninst-4fb1f52 snapshot segfaults on close().
  // Symtab::closeSymtab(the_symtab);

  the_symtab = NULL;
}


void
printFunc(FunctionBase *base, int &depth)
{
  FunctionBase *parent = base->getInlinedParent();
  InlinedFunction *ifunc = NULL;

  if (parent) {
    //Recursively print parents first
    printFunc(parent, depth);

    //If we have a parent, then we are an InlinedFunction
    ifunc = static_cast<InlinedFunction *>(base);
  }
  for (int i = 0; i < depth; i++) {
    cout << "  ";
  }

  vector<string> names = base->getAllPrettyNames();
  if (names.empty()) {
    cerr << "Unable to find any names for function" << endl;
    exit(-1);
  }
  string name = names[0];

  cout << name;
  if (ifunc) {
    pair<string, Offset> callsite = ifunc->getCallsite();
    cout << ", Inlined from callsite " << callsite.first << ":" << callsite.second;
  }
  cout << endl;

  depth = depth + 1;
}


CallingContext *
getStaticCallChain(FunctionBase *base, CallingContext *succ)
{
  FunctionBase *parent = base->getInlinedParent();
  InlinedFunction *ifunc = NULL;

  if (parent) {
    ifunc = static_cast<InlinedFunction *>(base);

    vector<string> names = base->getAllPrettyNames();

    string name;
    if (names.empty()) {
      name = "~unknown procedure~";
    } else {
      name = names[0];
    }

    pair<string, Offset> callsite;
    if (ifunc) {
      callsite = ifunc->getCallsite();
    }

    // build chain with outermost context at front
    CallingContext *chain =
      getStaticCallChain(parent,
			 new CallingContext(name, callsite.first,
					    (unsigned int) callsite.second, succ));
    return chain;
  }

  return succ;
}


CallingContext *
inliningAtAddr(Symtab *symt, unsigned long addr)
{
  FunctionBase *base;
  bool result = symt->getContainingInlinedFunction(addr, base);
  CallingContext * callChain = NULL;

  if (result) {
#if 0
    int depth = 0;
    printFunc(base, depth);
#endif

    callChain = getStaticCallChain(base, NULL);
#if 0
    if (callChain) {
      cout << std::hex << addr << std::dec << ": " << endl;
      int indent = 0;
      for (CallingContext *it = callChain; it != NULL; it = it->Next())  {
	printf("%*s", indent, "");
	it->Dump();
	indent += 2;
      }
      cout << endl;
    }
#endif
  }
  return callChain;
}


CallingContext *
analyzeInlining(unsigned long addr)
{
  if (the_symtab == NULL) {
    return NULL;
  }

  return inliningAtAddr(the_symtab, addr);
}
