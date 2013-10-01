//
//  Matt Legendre's program to display the sequence of nested inlines
//  from SymtabAPI.  Requires a recent dyninst with Matt's patches.
//  Slightly modified to take the file name from the command line.
//
//  Usage: matt-inline filename start end
//
//  where start and end are written in hex with leading 0x.
//  Analyzes range [start, end).
//
//  $Id$
//

#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <string>
#include <set>

#include "Symtab.h"
#include "Function.h"

using namespace Dyninst;
using namespace SymtabAPI;
using namespace std;


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
	cout << "    ";
    }

    vector<string> names = base->getAllPrettyNames();
    if (names.empty()) {
	cerr << "unable to find any names for function" << endl;
	exit(1);
    }
    string name = names[0];

    cout << name;
    if (ifunc) {
	pair<string, Offset> callsite = ifunc->getCallsite();
	cout << ": inlined from " << callsite.first << ":" << callsite.second;
    }
    cout << endl;

    depth = depth + 1;
}

void
inliningAtAddr(Symtab *symt, unsigned long addr)
{
    bool result;
    FunctionBase *base;
    int depth;

    cout << "0x" << std::hex << addr << std::dec << ": ";

    result = symt->getContainingInlinedFunction(addr, base);
    if (! result) {
	cout << endl;
	return;
    }

    depth = 0;

    printFunc(base, depth);
}

void
usage(int status)
{
    cout << "usage: matt-inline filename start end\n\n"
	 << "where start and end are written in hex with leading '0x'.\n"
	 << "analyzes range [start, end).\n";

    exit(status);
}

int
main(int argc, char **argv)
{
    unsigned long start;
    unsigned long end;
    Symtab *symt;

    if (argc < 4) {
	usage(0);
    }
    if (sscanf(argv[2], "0x%lx", &start) < 1) {
	cerr << "unable to read start address: " << argv[2] << "\n\n";
	usage(1);
    }
    if (sscanf(argv[3], "0x%lx", &end) < 1) {
	cerr << "unable to read end address: " << argv[3] << "\n\n";
	usage(1);
    }

    bool result = Symtab::openFile(symt, argv[1]);
    if (! result) {
	cerr << "unable to open file: " << argv[1] << "\n";
	return 1;
    }

    symt->parseTypesNow();

    for (unsigned long i = start; i < end; i++) {
	inliningAtAddr(symt, i);
    }

    return 0;
}
