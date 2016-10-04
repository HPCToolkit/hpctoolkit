// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
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

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   XML adaptor for the program structure file (PGM)
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************ System Include Files ******************************

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <iostream> 
using std::cerr;
using std::endl;

#include <string>
using std::string;

//************************* User Include Files *******************************

#include "PGMReader.hpp"
#include "XercesUtil.hpp"

//*********************** Xerces Include Files *******************************

#include <xercesc/util/XMLString.hpp>
using XERCES_CPP_NAMESPACE::XMLString;

//************************ Forward Declarations ******************************

//****************************************************************************

namespace Prof {

namespace Struct {

// Simple sanity check for struct file that we can open it and it
// begins with '<?xml'.  Otherwise, the xerces error messages are
// rather unhelpful.
//
// FIXME: better to check for DOCTYPE tag:
// <!DOCTYPE HPCToolkitStructure
//
#define BUF_SIZE  10
static void
xmlSanityCheck(const char *filenm, string & docType)
{
  char buf[BUF_SIZE];

  int fd = open(filenm, O_RDONLY);
  if (fd < 0) {
    cerr << "unable to open " << docType << " file: '" << filenm << "': "
	 << strerror(errno) << endl;
    exit(1);
  }

  memset(buf, 0, BUF_SIZE);
  ssize_t ret = read(fd, buf, 5);
  if (ret < 0) {
    cerr << "unable to read " << docType << " file: '" << filenm << "': "
	 << strerror(errno) << endl;
    exit(1);
  }

  if (strncasecmp(buf, "<?xml", 5) != 0) {
    cerr << "unable to parse " << docType << " file: '" << filenm << "': "
	 << "not an xml file" << endl;
    exit(1);
  }

  close(fd);
}


void
readStructure(Struct::Tree& structure, 
	      const std::vector<string>& structureFiles,
	      PGMDocHandler::Doc_t docty, 
	      DocHandlerArgs& docargs)
{
  if (structureFiles.empty()) { return; }

  InitXerces();

  for (uint i = 0; i < structureFiles.size(); ++i) {
    const string& fnm = structureFiles[i];
    read_PGM(structure, fnm.c_str(), docty, docargs);
  }

  FiniXerces();
}


void
read_PGM(Struct::Tree& structure,
	 const char* filenm,
	 PGMDocHandler::Doc_t docty,
	 DocHandlerArgs& docHandlerArgs)
{
  if (!filenm || filenm[0] == '\0') {
    return;
  }

  string fpath = filenm;
  string docType = PGMDocHandler::ToString(docty);

  xmlSanityCheck(filenm, docType);

  if (!fpath.empty()) {
    try {
      SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();
      
      parser->setFeature(XMLUni::fgSAX2CoreValidation, true);
      parser->setFeature(XMLUni::fgXercesDynamic, true);
      parser->setFeature(XMLUni::fgXercesValidationErrorAsFatal, true);
      
      PGMDocHandler* handler = new PGMDocHandler(docty, &structure, 
						 docHandlerArgs);
      parser->setContentHandler(handler);
      parser->setErrorHandler(handler);
	  
      parser->parse(fpath.c_str());

      if (parser->getErrorCount() > 0) {
	DIAG_Throw("ignoring " << fpath << " because of previously reported parse errors.");
      }
      delete handler;
      delete parser;
    }
    catch (const SAXException& x) {
      DIAG_Throw("parsing '" << fpath << "'" << 
		 XMLString::transcode(x.getMessage()));
    }
    catch (const PGMException& x) {
      DIAG_Throw("reading '" << fpath << "'" << x.message());
    }
    catch (...) {
      DIAG_EMsg("While processing '" << fpath << "'...");
      throw;
    };
  } 
  else {
    DIAG_Throw("Could not open " << PGMDocHandler::ToString(docty) 
	       << " file '" << filenm << "'.");
  }
}


} // namespace Util

} // namespace Analysis
