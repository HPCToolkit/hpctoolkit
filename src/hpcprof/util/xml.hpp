// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_UTIL_XML_H
#define HPCTOOLKIT_PROFILE_UTIL_XML_H

#include <iosfwd>
#include <string>

namespace hpctoolkit::util {

/// Alternative to std::quoted for XML outputs.
class xmlquoted {
public:
  xmlquoted(std::string_view s, bool q = true);

  friend std::ostream& operator<<(std::ostream&, const xmlquoted&);

private:
  std::string_view str;
  bool addquotes;
};

std::ostream& operator<<(std::ostream&, const xmlquoted&);

}

#endif  // HPCTOOLKIT_PROFILE_UTIL_XML_H
