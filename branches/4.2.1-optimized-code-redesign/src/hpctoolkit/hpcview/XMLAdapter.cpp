// $Id$
// -*-C++-*-
// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// 
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
// 
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage. 
// 
// ******************************************************* EndRiceCopyright *

// ----------------------------------------------------------------------
//
// XMLStr.C
//   the implementation for a string converter class which converts a
//   c-style string to XMLCh* or vice versa.
//
// ----------------------------------------------------------------------

//************************ System Include Files ******************************

#include <iostream>
#include <ostream>

#ifdef NO_STD_CHEADERS
# include <stdlib.h>
#else
# include <cstdlib>
using std::wcstombs; // For compatibility with non-std C headers
using std::mbstowcs;
#endif

//************************* User Include Files *******************************

#include "XMLAdapter.h"
#include <lib/support/String.h>
#include <lib/support/Trace.h>

//************************ Forward Declarations ******************************

using std::cerr;
using std::endl;

//****************************************************************************

char* XMLStrToStr(const XMLCh* xmlStr) {
  
  // see if our XMLCh and wchar_t as the same on this platform
  const bool isSameSize = (sizeof(XMLCh) == sizeof(wchar_t));

  // get the actual number of chars
  unsigned int realLen = 0;
  const XMLCh* tmpPtr = xmlStr;
  while (*(tmpPtr++))
    realLen++;
  
  // create a tmp buffer
  wchar_t* tmpSource = new wchar_t[realLen + 1];
  if (isSameSize)
    memcpy(tmpSource, xmlStr, realLen * sizeof(wchar_t));
  else
    for (unsigned int index = 0; index < realLen; index++)
      tmpSource[index] = (wchar_t)xmlStr[index];
  tmpSource[realLen] = 0;

  // see now many chars we need to transcode this guy
  const unsigned int targetLen = wcstombs(0, tmpSource, 0);

  // allocate out storage member
  char* str = new char[targetLen + 1];

  //  And transcode our temp source buffer to the local buffer. Cap it
  //  off since the converter won't do it (because the null is beyond
  //  where the target will fill up.)
  wcstombs(str, tmpSource, targetLen);
  str[targetLen] = 0;

  delete [] tmpSource;          // delete the temporary buffer

  return str;                   // return the string
}

XMLCh* 
XMLStr(const char* str) {

  // see if our XMLCh and wchar_t as the same on this platform
  const bool isSameSize = (sizeof(XMLCh) == sizeof(wchar_t));
  
  int len = strlen(str);

  // create a temporary wchar_t buffer and the result buffer
  wchar_t* tmpBuf = new wchar_t[len + 1];
  XMLCh* xmlStr = new XMLCh[len + 1];

  // copy the c-style string into the temporary buffer
  mbstowcs(tmpBuf, str, len);

  // copy from temporary buffer to the result buffer
  if (isSameSize)
    memcpy(xmlStr, tmpBuf, len * sizeof(wchar_t));
  else
    for (int index = 0; index < len; index++)
      xmlStr[index] = (XMLCh) tmpBuf[index];
  xmlStr[len] = 0;

  delete [] tmpBuf;             // delete the temporary buffer

  return xmlStr;                // return the XML string
}

bool 
isXMLStrSame(const XMLCh* xmlStr1, const XMLCh* xmlStr2) 
{
  while (*xmlStr1) {
    if (*xmlStr1 != *xmlStr2)
      return false;
    xmlStr1++;
    xmlStr2++;
  }
  return (!(*xmlStr2));
}

// BEWARE:: toString returns its result via the global variable toStringResult
static String toStringResult; 
static void
toString(const XMLCh* xmlStr) 
{
  // see if our XMLCh and wchar_t as the same on this platform
  const bool isSameSize = (sizeof(XMLCh) == sizeof(wchar_t));

  // get the actual number of chars
  unsigned int realLen = 0;
  const XMLCh* tmpPtr = xmlStr;
  while (*(tmpPtr++))
    realLen++;
  
  // create a tmp buffer
  wchar_t* tmpSource = new wchar_t[realLen + 1];
  if (isSameSize)
    memcpy(tmpSource, xmlStr, realLen * sizeof(wchar_t));
  else
    for (unsigned int index = 0; index < realLen; index++)
      tmpSource[index] = (wchar_t)xmlStr[index];
  tmpSource[realLen] = 0;

  // see now many chars we need to transcode this guy
  const unsigned int targetLen = wcstombs(0, tmpSource, 0);

  // allocate out storage member
  char* str = new char[targetLen + 1];

  //  And transcode our temp source buffer to the local buffer. Cap it
  //  off since the converter won't do it (because the null is beyond
  //  where the target will fill up.)
  wcstombs(str, tmpSource, targetLen);
  str[targetLen] = 0;

  delete [] tmpSource;          // delete the temporary buffer

  toStringResult = str; 
  delete[] str; 
}

String
getAttr(AttributeList& attributes, int i) 
{
  const XMLCh* const xmlStr = attributes.getValue((unsigned int) i); 
  String s = "";
  if (xmlStr) {
    toString(xmlStr); 
    s = toStringResult;
  }
  return s; 
}

String 
getAttr(AttributeList& attributes, const XMLCh* const name)
{
  const XMLCh* const xmlStr = attributes.getValue(name); 
  String s = "";
  if (xmlStr) {
    toString(xmlStr); 
    s = toStringResult;
  }
  return s;
}

String 
XMLStrToString(const  XMLCh* xmlStr) 
{
  toString(xmlStr); 
  String s = toStringResult; 
  return s; 
}

void 
ReportException(const char* msg, const XMLException& toCatch)
{
  cerr << msg << "\n"
       << "       " << XMLStrToString(toCatch.getMessage())
       << endl;
}

void 
ReportException(const char* msg, const SAXException& toCatch)
{
  cerr << msg << "\n"
       << "       " << XMLStrToString(toCatch.getMessage())
       << endl;
}

// ---------------------------------------------------------------------------
//
//  outputContent  - Write document content from a DOMString to a C++ ostream.
//                   Escape the XML special characters (<, &, etc.) unless this
//                   is suppressed by the command line option.
//
// ---------------------------------------------------------------------------
void 
outputContent(std::ostream& target, const DOMString &toWrite, bool doEscapes)
{
    
    if (doEscapes == false)
    {
        target << toWrite;
    }
     else
    {
        int            length = toWrite.length();
        const XMLCh*   chars  = toWrite.rawBuffer();
        
        int index;
        for (index = 0; index < length; index++)
        {
            switch (chars[index])
            {
            case chAmpersand :
                target << "&amp;";
                break;
                
            case chOpenAngle :
                target << "&lt;";
                break;
                
            case chCloseAngle:
                target << "&gt;";
                break;
                
            case chDoubleQuote :
                target << "&quot;";
                break;
                
            default:
                // If it is none of the special characters, print it as such
                target << toWrite.substringData(index, 1);
                break;
            }
        }
    }

    return;
}


// ---------------------------------------------------------------------------
//
//  ostream << DOMString    Stream out a DOM string.
//                          Doing this requires that we first transcode
//                          to char * form in the default code page
//                          for the system
//
// ---------------------------------------------------------------------------
std::ostream& 
operator<<(std::ostream& target, const DOMString& s)
{
    char *p = s.transcode();
    target << p;
    delete [] p;
    return target;
}


// so a DOM_Node can easily be output from a debugger
std::ostream&
dumpDomNode(DOM_Node& node, std::ostream& os)
{
  os << node;
  return os;
}


void
ddumpDomNode(DOM_Node& node)
{
  dumpDomNode(node, std::cerr);
}


// ---------------------------------------------------------------------------
//
//  ostream << DOM_Node   
//
//                Stream out a DOM node, and, recursively, all of its children.
//                This function is the heart of writing a DOM tree out as
//                XML source.  Give it a document node and it will do the whole thing.
//
// ---------------------------------------------------------------------------
std::ostream& 
operator<<(std::ostream& target, DOM_Node& toWrite)
{
    // Get the name and value out for convenience
    DOMString   nodeName = toWrite.getNodeName();
    DOMString   nodeValue = toWrite.getNodeValue();

	switch (toWrite.getNodeType())
    {
		case DOM_Node::TEXT_NODE:
        {
	    outputContent(target, nodeValue, false);
            break;
        }

        case DOM_Node::PROCESSING_INSTRUCTION_NODE :
        {
            target  << "<?"
                    << nodeName
                    << ' '
                    << nodeValue
                    << "?>";
            break;
        }

        case DOM_Node::DOCUMENT_NODE :
        {
            // Bug here:  we need to find a way to get the encoding name
            //   for the default code page on the system where the
            //   program is running, and plug that in for the encoding
            //   name.  
	  //    target << "<?xml version='1.0' encoding='ISO-8859-1' ?>\n";
            DOM_Node child = toWrite.getFirstChild();
            while( child != 0)
            {
                target << child << endl;
                child = child.getNextSibling();
            }

            break;
        }

        case DOM_Node::ELEMENT_NODE :
        {
            // Output the element start tag.
            target << '<' << nodeName;

            // Output any attributes on this element
            DOM_NamedNodeMap attributes = toWrite.getAttributes();
            int attrCount = attributes.getLength();
            for (int i = 0; i < attrCount; i++)
            {
                DOM_Node  attribute = attributes.item(i);

                target  << ' ' << attribute.getNodeName()
                        << " = \"";
                        //  Note that "<" must be escaped in attribute values.
                        outputContent(target, attribute.getNodeValue(), false);
                        target << '"';
            }

            //
            //  Test for the presence of children, which includes both
            //  text content and nested elements.
            //
            DOM_Node child = toWrite.getFirstChild();
            if (child != 0)
            {
                // There are children. Close start-tag, and output children.
                target << ">";
                while( child != 0)
                {
                    target << child;
                    child = child.getNextSibling();
                }

                // Done with children.  Output the end tag.
                target << "</" << nodeName << ">";
            }
            else
            {
                //
                //  There were no children.  Output the short form close of the
                //  element start tag, making it an empty-element tag.
                //
                target << "/>";
            }
            break;
        }

        case DOM_Node::ENTITY_REFERENCE_NODE:
        {
            DOM_Node child;
            for (child = toWrite.getFirstChild(); child != 0;
		      child = child.getNextSibling())
                target << child;
            break;
        }

        case DOM_Node::CDATA_SECTION_NODE:
        {
            target << "<![CDATA[" << nodeValue << "]]>";
            break;
        }

        case DOM_Node::COMMENT_NODE:
        {
            target << "<!--" << nodeValue << "-->";
            break;
        }

	// Node types defined with Xerces1_4_0
        case DOM_Node::DOCUMENT_TYPE_NODE:
            {
	      DOM_Node child = toWrite.getFirstChild();
	      while( child != 0)
		{
		  target << child << endl;
		  child = child.getNextSibling();
		}
	      break;
	    }

        case DOM_Node::DOCUMENT_FRAGMENT_NODE:
        case DOM_Node::NOTATION_NODE:
        case DOM_Node::XML_DECL_NODE:
        {
            target << "<!--" << nodeValue << "-->";
            break;
        }
        default:
            cerr << "Unrecognized node type = "
                 << (long)toWrite.getNodeType() << endl;
    }
	return target;
}

SAXHandlerBase::SAXHandlerBase(bool verb) : HandlerBase(), verbose(verb)
{
}
 
static void 
Report(std::ostream &cerr, 
       const char *prefix, const SAXParseException& e)
{
  cerr << XMLStrToString(e.getSystemId()) 
       << ", line " << e.getLineNumber()
       << ", char " << e.getColumnNumber() << " "  
       << prefix << ": " 
       << XMLStrToString(e.getMessage()) << "." << endl; 
}

void 
SAXHandlerBase::error(const SAXParseException& e)
{
  IFTRACE << "PROFILEDocHandler::error" << endl; 
  lastErrorLine = e.getLineNumber(); 
  lastErrorCol = e.getColumnNumber(); 
  if (verbose) { 
    Report(cerr, "Parse Error", e); 
  }
  throw e; 
}

void 
SAXHandlerBase::fatalError(const SAXParseException& e)
{
  error(e); 
}

void 
SAXHandlerBase::warning(const SAXParseException& e)
{
  IFTRACE << "PROFILEDocHandler::warning" << endl; 
  lastWarningLine = e.getLineNumber(); 
  lastWarningCol = e.getColumnNumber(); 
}

/*
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 1999-2000 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Xerces" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache\@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation, and was
 * originally based on software copyright (c) 1999, International
 * Business Machines, Inc., http://www.ibm.com .  For more information
 * on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 */

// Global streaming operator for DOMString is defined in DOMPrint.cpp
extern std::ostream& operator<<(std::ostream& target, const DOMString& s);


DOMTreeErrorReporter::DOMTreeErrorReporter(String &userFile, String &tmpFile, 
					   int prefixLines)
{
  realUserFile = userFile;
  tmpUserFile = tmpFile;
  tmpPrefixLines = prefixLines;
}

void DOMTreeErrorReporter::error(const SAXParseException& toCatch)
{
  fatalError(toCatch);
}

void DOMTreeErrorReporter::warning(const SAXParseException& toCatch)
{
  DOMString dfile(toCatch.getSystemId());
  String file(dfile.transcode());

  int line = toCatch.getLineNumber();
  if (tmpUserFile == file) {
    file = realUserFile;
    line -= tmpPrefixLines;
  }
    
  cerr << "Non-fatal XML parser error encountered in file \"" << file
       << "\", line " << line
       << ", column " << toCatch.getColumnNumber() << endl 
       << "  " << DOMString(toCatch.getMessage()) << endl;
}

void DOMTreeErrorReporter::fatalError(const SAXParseException& toCatch)
{
   DOMString dfile(toCatch.getSystemId());
  String file(dfile.transcode());

  int line = toCatch.getLineNumber();
  if (tmpUserFile == file) {
    file = realUserFile;
    line -= tmpPrefixLines;
  }
    
  cerr << "Fatal XML parser error encountered in file \"" << file
       << "\", line " << line
       << ", column " << toCatch.getColumnNumber() << endl 
       << "  " << DOMString(toCatch.getMessage()) << endl;

  exit(-1);
}

void DOMTreeErrorReporter::resetErrors()
{
    // No-op in this case
}


