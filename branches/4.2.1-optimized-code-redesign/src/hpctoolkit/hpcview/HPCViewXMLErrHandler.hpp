#ifndef HPCViewXMLErrHandler_h
#define HPCViewXMLErrHandler_h

//*********************** Xerces Include Files *******************************

#include <xercesc/sax/ErrorHandler.hpp> 
using XERCES_CPP_NAMESPACE::ErrorHandler;

#include <xercesc/sax/SAXParseException.hpp>
using XERCES_CPP_NAMESPACE::SAXParseException;

//************************* User Include Files *******************************

#include <lib/support/String.hpp>

//****************************************************************************

class HPCViewXMLErrHandler : public ErrorHandler {
public:
  HPCViewXMLErrHandler(String &_userFile, String &_tmpFile, int _numPrefixLines, bool _verbose) : 
    userFile(_userFile), tmpFile(_tmpFile), numPrefixLines(_numPrefixLines), verbose(_verbose) {}; 

  // error handler interface
  void error(const SAXParseException& e);
  void fatalError(const SAXParseException& e);
  void warning(const SAXParseException& e);
  void resetErrors(){ };
  static void report(std::ostream &estream, const char *prefix, 
		     const char *fileType, const SAXParseException& e, 
		     const char *alternateFile = 0, int numPrefixLines = 0);

private:
  String userFile;
  String tmpFile;
  int numPrefixLines;
  bool verbose;
};

#endif
