// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "xml.hpp"

#include <ostream>

using namespace hpctoolkit::util;

xmlquoted::xmlquoted(std::string_view s, bool q)
  : str(s), addquotes(q) {};

std::ostream& hpctoolkit::util::operator<<(std::ostream& os, const xmlquoted& q) {
  if(q.addquotes) os << '"';
  for(const char& c: q.str) {
    if(c == '"') os << "\\\"";
    else if(c == '>') os << "&gt;";
    else if(c == '<') os << "&lt;";
    else if(c == '&') os << "&amp;";
    else os << c;
  }
  if(q.addquotes) os << '"';
  return os;
}
