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

//*************************************************************************** 
// 
// File: 
//    String.h
//
// Purpose:
//    [A brief statement of the purpose of the file.]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// History:
//    950920 - curetonk - initial revision
//
//***************************************************************************

#ifndef String_h
#define String_h

//************************** System Include Files ***************************

#ifdef NO_STD_CHEADERS
# include <string.h>
# include <ctype.h>
#else
# include <cstring>
using std::strlen; // For compatibility with non-std C headers
using std::strcpy;
using std::strcmp;
using std::strncmp;
using std::strerror;
using std::memset;
using std::memcpy;
using std::size_t;
# include <cctype>
using std::isdigit; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************

#include <include/general.h>

//************************ Enumeration Definitions **************************

//***************************************************************************
//
//  Enumeration:
//     StringStripLocation
//
//  Enumeration Members:
//     StringStripLeading  - strip leading whitespace. 
//     StringStripTrailing - strip trailing whitespace. 
//     StringStripBoth     - strip leading and trailing whitespace. 
//     StringStripAll      - strip all whitespace.
//
//  Purpose:
//     Determine what whitespace to remove from the string.
//
//  History:
//     950920 - curetonk - initial revision
//
//***************************************************************************
enum StringStripLocation 
{ 
   StringStripLeading, 
   StringStripTrailing, 
   StringStripBoth, 
   StringStripAll 
};

//***************************************************************************
//
//  Enumeration:
//     StringSearchDirection
//
//  Enumeration Members:
//     StringSearchForward  - search from position to end of string. 
//     StringSearchBackward - search from position to beginning of string.
//
//  Purpose:
//     The direction in which to search the string.
//
//  History:
//     950920 - curetonk - initial revision
//
//***************************************************************************
enum StringSearchDirection 
{ 
   StringSearchForward, 
   StringSearchBackward 
};

//***************************************************************************
//
//  Enumeration:
//     StringInclusive
//
//  Enumeration Members:
//     enumName1 -  description of enumName1
//     enumName2 -  description of enumName2
//
//  Purpose:
//     [The purpose of the enumeration, i.e., what it is used for.]
//
//  History:
//     950920 - curetonk - initial revision
//
//***************************************************************************
enum StringInclusive 
{ 
   StringOf, 
   StringNotOf 
};

//**************************** Class Definitions ****************************

//***************************************************************************
//
//  Class:
//     String
//
//  Base Classes:
//     none
//
//  Class Data Members:
//     char* stringText (private) - the character string.
//     unsigned int stringSize (private) - the size of the character string.
//
//  Purpose:
//
//  History:
//     950920 - curetonk - initial revision adapted from code found on 
//                         the internet.
//
//***************************************************************************
class String
{
   public:
      static const int UNKNOWN_POSITION;
        // A position return value that indicates not found.

      static const unsigned int MAX_SIZE;
        // A hypothetical maximum String size.

      //----------------------------------------
      // Constructors
      //----------------------------------------
      String(const char* newString = "", int theLength = UNKNOWN_POSITION);
        // Construct String from a known C-string and an optional length.
        // Note that this is the default constructor.

      String(char aChar);
        // Construct from a single character.

      String(const char* string1, const char* string2);
        // Construct from a pair of C-strings.

      String(char fillChar, unsigned int length);
        // Construct from a replicated character.
  
      String(double d); 
        // construct from double

      String(long i); 
        // construct from int/long value 

      String(unsigned long i, bool hex = false); 
        // construct from unsigned long value (setting 'hex' converts
        // to hex representation, (e.g. pointer))

#ifdef ARCH_USE_LONG_LONG // FIXME
      String(long long i);
      String(unsigned long long i, bool hex = false);
#endif
      
      String(const String& copyString);
        // Copy constructor.

      //----------------------------------------
      // Destructor
      //----------------------------------------
      ~String( );

      //----------------------------------------
      // NULL test ("" is the empty string)
      //----------------------------------------
      bool Empty() const { return (stringText[0] == '\0'); }
  
      //----------------------------------------
      // Size related functions
      //----------------------------------------
      unsigned int Length() const;
      void Resize(unsigned int newSize);
      unsigned int Size() const;

      //----------------------------------------
      // Formatting 
      //----------------------------------------
      void LeftFormat(int width);  
      void RightFormat(int width);  

      //----------------------------------------
      // Case conversion - returns a new string
      //----------------------------------------
      String ToUpper() const;
      String ToLower() const;

      //----------------------------------------
      // Positional operator
      //----------------------------------------
      // Return the (read-only) character in the string at a given offset.
      char operator[] (unsigned int position) const;

      // Return the (modifiable) character in the string at a given offset.
      char& operator[] (unsigned int position);

      //----------------------------------------
      // Type conversion
      //----------------------------------------
      operator const char*() const;

      //----------------------------------------
      // Assignment operators
      //----------------------------------------
      // Assign to this.
      String& operator=(const String& theString);
      String& operator=(const char* charString);
      String& operator=(char aChar);

      // Append another string to this.
      String& operator+=(const String& theString);
      String& operator+=(const char* charString);
      String& operator+=(char aChar);
      
      // Concatenate two strings - returns a new string
      friend String operator+(const String& s1, const String& s2);
      friend String operator+(const String& s1, const char* charString);

      //----------------------------------------
      // Relational operators
      //----------------------------------------

      // Compare two strings for equality.
      friend bool operator==(const String& s1, const String& s2);
      friend bool operator==(const String& s1, const char* charString);

      // Compare two strings for inequality.
      friend bool operator!=(const String& s1, const String& s2);
      friend bool operator!=(const String& s1, const char* charString);

      // Compare two strings for exclusive alphanumeric precedence. 
      friend bool operator<(const String& s1, const String& theString);
      friend bool operator<(const String& s1, const char* charString);

      // Compare two strings for exclusive alphanumeric antecedence.
      friend bool operator>(const String& s1, const String& s2);
      friend bool operator>(const String& s1, const char* charString);

      // Compare two strings for inclusive alphanumeric precedence. 
      friend bool operator<=(const String& s1, const String& s2);
      friend bool operator<=(const String& s1, const char* charString);

      // Compare two strings for inclusive alphanumeric antecedence.
      friend bool operator>=(const String& s1, const String& s2);
      friend bool operator>=(const String& s1, const char* charString);

      //----------------------------------------
      // Searching
      //----------------------------------------
      // Find the position of the target substring within this string.
      int FindSubstring(const char* target, 
                        StringSearchDirection direction = StringSearchForward, 
                        unsigned int startPosition = 0) const;
      
      // Find the position of any character in the target string.
      int FindChar(const char* target, 
                   StringInclusive isOf = StringOf, 
                   StringSearchDirection direction = StringSearchForward, 
                   unsigned int startPosition = 0) const;
      int FindChar(char target, 
                   StringInclusive isOf = StringOf, 
                   StringSearchDirection direction = StringSearchForward, 
                   unsigned int startPosition = 0) const;
      
      //----------------------------------------
      // Editing
      //----------------------------------------
      // Insert the new string into this at the given position.
      String& Insert(const char* charString, unsigned int insertPosition);
      
      // Remove size characters starting with the position.
      String& Remove(unsigned int removePosition, 
                     unsigned int removeSize = MAX_SIZE);
      
      // Replace size characters with the new string.
      String& Replace(const char* charString,
                      unsigned int replacePosition, 
                      unsigned int replaceSize);
      
      // Remove the given characters from the given location.
      // We default to stripping leading and trailing whitespace.
      String& Strip(StringStripLocation s = StringStripBoth,
                    const char* sTarget = " \t");

      //----------------------------------------
      // Dumping
      //----------------------------------------
      void Dump(const char* indent = "") const;

   protected:
      char* stringText;
      unsigned int stringSize;
      void VerifySizeForLength(unsigned int length, bool keep);
      static bool CharIsOfString(char aChar, const char* buffer);
};    


// ****************************************************************************
// classes/fcts/methods necessary for STL templates
class StringLt {
public:
  // strict (less-than) operator for Strings
  int operator()(const String& s1, const String& s2 ) const 
    { return s1 < s2; }
};

class StringEq {
public:
  // strict comparison (equal) operator for Strings
  int operator()(const String& s1, const String& s2 ) const 
    { return s1 == s2; }
};

extern size_t HashString(const String &s); 

class StringHash {
public:
  // strict comparison (equal) operator for Strings
  size_t operator()(const String& s) const {
    return HashString(s); 
  }
};

#endif 
