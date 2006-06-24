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

// ***************************************************************************
//
// File:
//    String.C
//
// Purpose:
//    [A brief statement of the purpose of the file.]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// History:
//    950920 - curetonk - initial revision adapted from code found on 
//                        the internet.
//
// ***************************************************************************

// ************************** System Include Files ***************************

#include <iostream>
#include <locale>  // STL: for 'hash'

#ifdef NO_STD_CHEADERS
# include <stdio.h>
# include <math.h>
#else
# include <cstdio> // for 'sprintf'
# include <cmath>
using namespace std; // For compatibility with non-std C headers
#endif

#include <unistd.h>

// *************************** User Include Files ****************************

#include "String.hpp"
#include "StringUtilities.hpp"
#include "Assertion.h"

// **************************** Static Functions *****************************

inline unsigned int BestStringSize(unsigned int length)
{
   return (length + 16) & ~0xF;
}

// ************************** Variable Definitions ***************************

const int String::UNKNOWN_POSITION = -1;
const unsigned int String::MAX_SIZE = 65535;

// ******************** String Member Functions - Public *********************

// ***************************************************************************
//  Constructors
// ***************************************************************************
String::String(char aChar)
{
   stringSize = BestStringSize(1);
   stringText = new char[stringSize];
   stringText[0] = aChar;
   stringText[1] = '\0';
}
    
String::String(const char* newString, int theLength)
{
   if (newString == 0)
   {// treat a null as an empty string
      newString = "";
      theLength = 0;
   }
   if (theLength == UNKNOWN_POSITION)
   {// calculate the length
      theLength = strlen(newString);
   }
   stringSize = BestStringSize(theLength);
   stringText = new char[stringSize];
   strncpy(stringText, newString, theLength);
   stringText[theLength] = '\0';
}

String::String(const char* string1, const char* string2)
{
   if (string1 == 0)
   {// treat a null as an empty string
      string1 = "";
   }
   if (string2 == 0)
   {// treat a null as an empty string
      string2 = "";
   }
   unsigned int length1 = strlen(string1);
   stringSize = BestStringSize(length1 + strlen(string2));
   stringText = new char[stringSize];
   strcpy(stringText,           string1);
   strcpy(stringText + length1, string2);
}

String::String(char fillChar, unsigned int length)
{
   stringSize = BestStringSize(length + 1);
   stringText = new char[stringSize];
   for (unsigned int i = 0; i < length; i++)
   {// fill in the string
       stringText[i] = fillChar;
   }
   stringText[length] = '\0';
}

String::String(const String& copyString)
{
  if (this == &copyString) {
    return; 
  } 
  stringSize = copyString.stringSize;
  stringText = new char[stringSize];
  strcpy(stringText, copyString.stringText);
}

void
String::ctor(long i) 
{
   int sign = 0;
   long absi = i;
   if (i < 0) {
      absi = -i; 
      sign = 1; 
   }  
   if (absi == 0) {
      stringSize = 2; 
   }  else {
      stringSize = 2 + (int) log10((double)absi) + sign; 
   } 
   stringText = new char[stringSize];
   itoa(i, stringText); 
}

void
String::ctor(unsigned long l, bool hex)
{
  if (hex) {
    // eraxxon: make this stuff obsolete
    // for turning up to 64 bit pointers into readable hex strings 
    //stringSize = 19;  // 0xhhhhhhhhhhhhhhhh format; close to log16(2^64) + 0x
    //ultohex(l, stringText); 
    
    int numSz = (l == 0) ? 1 : (int) log10((double)l); // no log16...
    stringSize = 4 + numSz; 
    stringText = new char[stringSize];
    sprintf(stringText, "%#lx", l);
  } else {
    int numSz = (l == 0) ? 1 : (int) log10((double)l);
    stringSize = 2 + numSz;
    stringText = new char[stringSize];
    sprintf(stringText, "%lu", l);
  }
}

#ifdef ARCH_USE_LONG_LONG // FIXME

String::String(long long l)
{
  // FIXME
}

String::String(unsigned long long l, bool hex)
{
  // FIXME: assume hex for now
  int numSz = (l == 0) ? 1 : (int) log10((double)l); // no log16...
  stringSize = 4 + numSz; 
  stringText = new char[stringSize];
  sprintf(stringText, "%#llx", l);
}

#endif

String::String(double d) 
{
  stringSize = 19;  // 0xhhhhhhhhhhhhhhhh format
  stringText = new char[stringSize];
  sprintf(stringText, "%.3f", d); 
}


//***************************************************************************
//  Destructors
//***************************************************************************
String::~String()
{
   delete[] stringText;
}

//***************************************************************************
//  Size related functions
//***************************************************************************
unsigned int String::Length() const 
{ 
   return strlen(stringText); 
}

void String::Resize(unsigned int newSize)
{
   unsigned int len = Length();
   if (newSize > len)
   {
      VerifySizeForLength(newSize, true);
      for (unsigned int i = len; i < newSize; i++) stringText[i] = ' ';
   }
   stringText[newSize] = '\0';
}         

unsigned int String::Size() const 
{ 
   return stringSize; 
}

void String::LeftFormat(int width) 
{ 
  BriefAssertion(width > 0); 
  unsigned int newLen = width + 1; 
  if (newLen <= Length()) {
     stringText[width] = '\0'; 
     return; 
  } 
  VerifySizeForLength(newLen, true);
  for (unsigned int i = Length(); i < (unsigned int)width; i++) {
    stringText[i] = ' '; 
  } 
  stringText[width] = '\0'; 
}

void String::RightFormat(int width) 
{ 
  BriefAssertion(width > 0); 
  int len = Length();   // number of chars before '\0'
  if (width <= len) {
     stringText[width] = '\0'; 
     return; 
  } 
  String s(' ', (unsigned int)width); 
  unsigned int p = width - len; 
  BriefAssertion(width - len >= 0); 
  for (int i = 0; stringText[i] != '\0'; i++, p++) {
    s[p] = stringText[i]; 
  } 
  BriefAssertion((p == (unsigned int)width)); 
  s[p] = '\0'; 
  (*this) = s; 
}


//***************************************************************************
//  Case conversion
//***************************************************************************
String String::ToLower() const
{
   String temp(*this);

   for (char* ptr = temp.stringText; *ptr; ptr++)
   {// walk through the new string
       if (isupper(*ptr))
       {// convert this character to lower case
           *ptr += 'a' - 'A';
       }
   }
   return temp;  
}

String String::ToUpper() const
{
   String temp(*this);
    
   for (char* ptr = temp.stringText; *ptr; ptr++)
   {// walk through the new string
       if (islower(*ptr))
       {// convert this character to upper case
           *ptr -= 'a' - 'A';
       }
   }
   return temp;  
}

//***************************************************************************
//  Positional operator
//***************************************************************************

char String::operator[](unsigned int position) const
{
   return *(stringText + position);
}

char& String::operator[](unsigned int position)
{
   return *(stringText + position);
}

//***************************************************************************
//  Type conversion
//***************************************************************************
String::operator const char*() const
{
   return stringText;
}

//***************************************************************************
//  Assignment operators
//***************************************************************************
String& String::operator=(const String& theString)
{
   if (&theString != this)
   {// not a no-op
      VerifySizeForLength(theString.Length(), false);
      strcpy(stringText, theString.stringText);
   }
   return *this;
}

String& String::operator=(const char* charString)
{
   if (charString == 0)
   {// treat a null as an empty string
      charString = "";
   }
   VerifySizeForLength(strlen(charString), false);
   strcpy(stringText, charString);
   return *this;
}
        
String& String::operator=(char aChar)
{
   VerifySizeForLength(1, false);
   stringText[0] = aChar;
   stringText[1] = '\0';
   return *this;
}
        
String& String::operator+=(const String& theString)
{
   unsigned int length = Length();
   VerifySizeForLength(length + theString.Length(), true);
   strcpy(stringText + length, theString.stringText);
   return *this;
}  

String& String::operator+=(const char* charString)
{
   if (charString != 0)
   {// there is a c-string to append
      unsigned int length = Length();
      VerifySizeForLength(length + strlen(charString), true);
      strcpy(stringText + length, charString);
   }
   return *this;
}
        
String& String::operator+=(char aChar)
{
   unsigned int length = Length();
   VerifySizeForLength(length + 1, true);
   stringText[length]     = aChar;
   stringText[length + 1] = '\0';
   return *this;
}
       
String operator+(const String& s1, const String& s2)
{
   String result(s1.stringText, s2.stringText);
   return result;
}

String operator+(const String& s1, const char* charString)
{
  String result(s1.stringText, charString);
  return result;
}

//***************************************************************************
//  Relational operators
//***************************************************************************
bool operator==(const String& s1, const String& s2)
{
   return (strcmp(s1.stringText, s2.stringText) == 0);
}

bool operator==(const String& s1, const char* charString)
{
   if (charString == 0)
   {// treat a null as an empty string
      charString = "";
   }
   return (strcmp(s1.stringText, charString) == 0);
}

bool operator!=(const String& s1, const String& s2)
{
   return (strcmp(s1.stringText, s2.stringText) != 0);
}

bool operator!=(const String& s1, const char* charString)
{
   if (charString == 0)
   {// treat a null as an empty string
      charString = "";
   }
   return (strcmp(s1.stringText, charString) != 0);
}

bool operator<(const String& s1, const String& s2)
{
   return (strcmp(s1.stringText, s2.stringText) < 0);
}

bool operator<(const String& s1, const char* charString)
{
   if (charString == 0)
   {// treat a null as an empty string
      charString = "";
   }
   return (strcmp(s1.stringText, charString) < 0);
}

bool operator>(const String& s1, const String& s2)
{
   return (strcmp(s1.stringText, s2.stringText) > 0);
}

bool operator>(const String& s1, const char* charString)
{
   if (charString == 0)
   {// treat a null as an empty string
      charString = "";
   }
   return (strcmp(s1.stringText, charString) > 0);
}

bool operator<=(const String& s1, const String& s2)
{
   return (strcmp(s1.stringText, s2.stringText) <= 0);
}

bool operator<=(const String& s1, const char* charString)
{
   if (charString == 0)
   {// treat a null as an empty string
      charString = "";
   }
   return (strcmp(s1.stringText, charString) <= 0);
}

bool operator>=(const String& s1, const String& s2)
{
   return (strcmp(s1.stringText, s2.stringText) >= 0);
}

bool operator>=(const String& s1, const char* charString)
{
   if (charString == 0)
   {// treat a null as an empty string
      charString = "";
   }
   return (strcmp(s1.stringText, charString) >= 0);
}

//***************************************************************************
//  Search functions
//***************************************************************************
int String::FindSubstring(const char* target, 
                          StringSearchDirection direction, 
                          unsigned int startPosition) const
{
   ProgrammingErrorIfNot(target);
   const unsigned int length = Length();
   const unsigned int targetLength = strlen(target);

   if (direction == StringSearchForward)
   {// forward search
      for (int place = startPosition; place + targetLength <= length; place++)
      {
         if (strncmp(target, stringText+place, targetLength) == 0)
         {
            return place;
         }
      }
   }
   else // if (direction == StringSearchBackward)
   {// backward search
      for (int place = MIN(startPosition, length - targetLength); place >= 0; place--)
      {
         if (strncmp(target, stringText+place, targetLength) == 0)
         {
            return place;
         }
      }
   }
   return UNKNOWN_POSITION;  // not found
}

int String::FindChar(const char* target, 
                     StringInclusive isOf,
                     StringSearchDirection direction, 
                     unsigned int startPosition) const
{
   ProgrammingErrorIfNot(target);
   const unsigned int length = Length();

   if (direction == StringSearchForward)
   {    
      for (unsigned int place = startPosition; place < length; place++)
      {
         bool match = CharIsOfString(stringText[place], target);
         if (match == (isOf == StringOf))
	 {// found the place
            return place;
	 }
      }
   }
   else // if (direction == StringSearchBackward) 
   {// search backwards
      for (int place = MIN(startPosition, length - 1); place >= 0; place--)
      {
         bool match = CharIsOfString(stringText[place], target);
         if (match == (isOf == StringOf))
	 {// found the place
            return place;
	 }
      }
   }
   return UNKNOWN_POSITION;  // not found
}

int String::FindChar(char target, 
                     StringInclusive isOf,
                     StringSearchDirection direction, 
                     unsigned int startPosition) const
{
   const unsigned int length = Length();

   if (direction == StringSearchForward)
   {    
      for (unsigned int place = startPosition; place < length; place++)
      {
         bool match = (stringText[place] == target);
         if (match == (isOf == StringOf))
	 {// found the place
            return place;
	 }
      }
   }
   else // if (direction == StringSearchBackward) 
   {// search backwards
      for (int place = MIN(startPosition, length - 1); place >= 0; place--)
      {
         bool match = (stringText[place] == target);
         if (match == (isOf == StringOf))
	 {// found the place
            return place;
	 }
      }
   }
   return UNKNOWN_POSITION;  // not found
}

//***************************************************************************
//  Editing functions
//***************************************************************************
String& String::Insert(const char* newString, unsigned int insertPosition)
{
   if (newString != 0)
   {// there is something to insert
      const unsigned int length = Length();
      ProgrammingErrorIfNot(insertPosition <= length);
      const unsigned int newStringLength = strlen(newString);

      VerifySizeForLength(length + newStringLength, true);

      const char* src = stringText + length;  // at null terminator pos.
      char* dst = stringText + length + newStringLength;  // new null term. pos.
      while (src >= stringText + insertPosition)
      {// shift the latter portion of the string up
         *dst-- = *src--;
      }
      dst = stringText + insertPosition;
      while (*newString)
      {// copy the new string into the gap
         *dst++ = *newString++;
      }
   }
   return *this;  
}
  
String& String::Remove(unsigned int removePosition, unsigned int removeSize)
{
   const unsigned int length = Length();
   ProgrammingErrorIfNot(removePosition <= length);
   removeSize = MIN(removeSize, length - removePosition);

   if (removeSize)
   {// there is something to be done
      char* dst = stringText + removePosition;  // first within gap
      const char* src = dst + removeSize;  // first after gap
      while (*src)
      {// shift the latter portion of the string down
         *dst++ = *src++;
      }
      *dst = '\0';
   }
   return *this;
}

String& String::Replace(const char* newString, 
                        unsigned int replacePosition, 
                        unsigned int replaceSize)
{
   ProgrammingErrorIfNot(newString);
    Remove(replacePosition, replaceSize);
    Insert(newString, replacePosition);
    return *this;
}
    
String& String::Strip(StringStripLocation strip, const char* target)
{
   ProgrammingErrorIfNot(target);
   unsigned int length = Length();

   if (strip != StringStripLeading)
   {// strip trailing white space
      while (length && CharIsOfString(stringText[length - 1], target))
      {// the last character should be stripped
          stringText[--length] = '\0';
      }
   }  

   if (strip != StringStripTrailing)
   {// strip leading white space
      int place = FindChar(target, StringNotOf);
      if (place != UNKNOWN_POSITION)
      {// shift the latter part of the string down
         for (unsigned int i = place; i <= length; i++)   
         {
            stringText[i - place] = stringText[i];
         }
         length -= place;
      }
      else
      {// the entire string should be stripped
         length = 0;
         *stringText = '\0';
      }
   }

   if (strip == StringStripAll)
   {// strip the remaining (dumbly)
      int place = FindChar(target, StringOf);
      while (place != UNKNOWN_POSITION)
      {
         Remove(place, 1);
      }
   }
      
   return *this;
}

//***************************************************************************
//  Dump function
//***************************************************************************
void String::Dump(const char* indent) const
{
   std::cout << indent << "String = " << stringText << "\n"
	     << indent << "String size = " << stringSize << "\n"
	     << std::endl;
}

//***************************************************************************
//  Helper functions
//***************************************************************************
void String::VerifySizeForLength(unsigned int length, bool keep)
{
   if (length >= stringSize)
   {// reallocate the buffer
       if (keep)
       {// keep what's there
          char* temp = stringText;
          stringSize = BestStringSize(length);
          stringText = new char[stringSize];
          strcpy(stringText, temp);
          delete[] temp;
       }
       else
       {// don't keep what's there
          delete[] stringText;
          stringSize = BestStringSize(length);
          stringText = new char[stringSize];
       }
   }
}

bool String::CharIsOfString(char aChar, const char* buffer)
{
   ProgrammingErrorIfNot(buffer);
   if (islower(aChar)) aChar = toupper(aChar);
   for (const char* ptr = buffer; *ptr; ptr++)
   {
      if (aChar == (islower(*ptr) ? toupper(*ptr) : *ptr))
      {
         return true;
      }
   }
   return false;  // not found
}

size_t 
HashString(const String &s) 
{
  using namespace std;
  
  locale loc;

  // Get a reference to the collate<char> facet
  const collate<char>& C =
#ifndef _RWSTD_NO_TEMPLATE_ON_RETURN_TYPE
    use_facet<collate<char> >(loc);
#else
    use_facet(loc, (collate<char>*) 0);
#endif

  const char* str = (const char*)s;
  return C.hash(str, str + s.Length());
} 
