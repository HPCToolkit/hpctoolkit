#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "symtabAPI/h/Symtab.h"
#include "symtabAPI/h/Archive.h"
#define logerror printf

using namespace DynSymtab;

static int mutatorTest(Symtab *symtab)
{
	/*********************************************************************************
		Dyn_Symtab::isExec()
	*********************************************************************************/	
	if(!symtab->isExec())
	{
		logerror("***** Error : reported File %s as shared\n", symtab->file().c_str());
		return -1;
	}
//	symtab->exportXML("file.xml");
	if(symtab->emitSymbols("./elffile"))
		cout << "wrote file successfully" << endl;
}

//extern "C" TEST_DLL_EXPORT int test1__mutatorMAIN(ParameterDict &param)
int main(int argc, char **argv)
{
	// dprintf("Entered test1_1 mutatorMAIN()\n");
	//string s = "/p/paradyn/development/giri/core/testsuite/i386-unknown-linux2.4/test1.mutatee_g++";
	//string s = "test1.mutatee_gcc";
	//string s = "stripped_g++";
	string s = argv[1]; 	
	Symtab *symtab;
	bool err = Symtab::openFile(symtab,s);
        //symtab = param["symtab"]->getPtr();
	// Get log file pointers
	//FILE *outlog = (FILE *)(param["outlog"]->getPtr());
	//FILE *errlog = (FILE *)(param["errlog"]->getPtr());
	//setOutputLog(outlog);
	//setErrorLog(errlog);
	// Read the program's image and get an associated image object
	// Run mutator code
	return mutatorTest(symtab);
}
