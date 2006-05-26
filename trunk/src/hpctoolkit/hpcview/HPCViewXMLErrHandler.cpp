//************************ System Include Files ******************************

#include <iostream> 
using std::cout;
using std::cerr;
using std::endl;


//************************* Xerces Include Files *******************************

#include <xercesc/util/XMLString.hpp> 
using XERCES_CPP_NAMESPACE::XMLString;

// #include <xercesc/sax/SAXParseException.hpp>
// using XERCES_CPP_NAMESPACE::SAXParseException;

//************************* User Include Files *******************************

#include "HPCViewXMLErrHandler.hpp"

const char *CONFIG = "CONFIGURATION";


//****************************************************************************
 
void HPCViewXMLErrHandler::report
(std::ostream &cerr, const char *prefix, const char *fileType, 
 const SAXParseException& e, const char *alternateFile, int prefixLines)
{
  const char *file = 
    (alternateFile) ? alternateFile : XMLString::transcode(e.getSystemId());

  cerr << prefix << ": processing " << fileType << " file \'" << file << "\'"
       << " at line " << e.getLineNumber() - prefixLines
       << ", character " << e.getColumnNumber() << ":" << endl << "\t"  
       << "XML parser: " 
       << XMLString::transcode(e.getMessage()) << "." << endl; 
}


void HPCViewXMLErrHandler::error(const SAXParseException& e)
{
  report(cerr, "hpcview non-fatal error", CONFIG, e, userFile, numPrefixLines); 
}

void 
HPCViewXMLErrHandler::fatalError(const SAXParseException& e)
{
  report(cerr, "hpcview fatal error", CONFIG, e, userFile, numPrefixLines); 
  throw e; 
}

void HPCViewXMLErrHandler::warning(const SAXParseException& e)
{
  report(cerr, "hpcview warning", CONFIG, e, userFile, numPrefixLines); 
}

