#ifndef HPCViewSAX2_h
#define HPCViewSAX2_h

# include <lib/support/String.h>

//************************* Xerces Include Files *******************************


#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
using XERCES_CPP_NAMESPACE::SAX2XMLReader;
using XERCES_CPP_NAMESPACE::XMLReaderFactory;
using XERCES_CPP_NAMESPACE::XMLUni;
using XERCES_CPP_NAMESPACE::SAXException;

#include <xercesc/sax2/DefaultHandler.hpp>
using XERCES_CPP_NAMESPACE::DefaultHandler;
using XERCES_CPP_NAMESPACE::Attributes;

#include <xercesc/sax/SAXParseException.hpp>
using XERCES_CPP_NAMESPACE::SAXParseException;

#include <xercesc/sax/ErrorHandler.hpp>
using XERCES_CPP_NAMESPACE::ErrorHandler;

extern String getAttr(const Attributes& attributes, int iU); 
extern String getAttr(const Attributes& attributes, const XMLCh* const attr); 

#endif
