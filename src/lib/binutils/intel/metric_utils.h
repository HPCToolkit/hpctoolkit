// -*-Mode: C++;-*- // technically C99

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
// Copyright ((c)) 2002-2022, Rice University
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

//==============================================================
// Copyright © 2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// =============================================================

#ifndef PTI_SAMPLES_UTILS_METRIC_UTILS_H_
#define PTI_SAMPLES_UTILS_METRIC_UTILS_H_

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__gnu_linux__) || defined(__APPLE__)
#include <cerrno>
#include <dlfcn.h>
#endif

#include <MD/metrics_discovery_api.h>
#include <memory>
#include <string>
#include <vector>

namespace utils {
namespace metrics {

inline std::vector<std::string> GetMDLibraryName() {
#if defined(_WIN32)
#if defined(_WIN64)
  return {"igdmd64.dll"};
#else
  return {"igdmd32.dll"};
#endif
#elif defined(__gnu_linux__)
  return {"libmd.so"};
#elif defined(__APPLE__)
  return {"libmd.dylib", "libigdmd.dylib"};
#else
#error "Unsupported platform!"
#endif
}

inline std::string GetPreferedLibraryPath() {
#ifdef _WIN32
  const std::string kKeyName = "SOFTWARE\\Intel\\MDF";
  const std::string kValueName = "DriverStorePath";

  HKEY key_handle = nullptr;
  LSTATUS status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, kKeyName.c_str(), 0, KEY_READ, &key_handle);
  if (status != ERROR_SUCCESS) {
    return "";
  }

  std::unique_ptr<std::remove_pointer<HKEY>::type, decltype(&RegCloseKey)> key(
      key_handle, RegCloseKey);

  DWORD buffer_size = MAX_PATH * sizeof(char);
  std::unique_ptr<BYTE[]> buffer(new BYTE[buffer_size]);

  status =
      RegQueryValueExA(key.get(), kValueName.c_str(), nullptr, nullptr, buffer.get(), &buffer_size);
  if (status != ERROR_SUCCESS) {
    return "";
  }

  return reinterpret_cast<char*>(buffer.get());
#elif defined(__linux__)
  return "/opt/intel/opencl";
#elif defined(__APPLE__)
  return "/System/Library/Extensions/AppleIntelBDWGraphicsMTLDriver.bundle/Contents/MacOS";
#else
  return "";
#endif
}

inline std::vector<std::string> GetMDLibraryPossiblePaths() {
  std::vector<std::string> paths;

  std::string prefered_path = GetPreferedLibraryPath();
  std::vector<std::string> library_names = GetMDLibraryName();

  for (auto& library_name : library_names) {
    if (!prefered_path.empty()) {
      paths.push_back(prefered_path + "/" + library_name);
    }
    paths.push_back(library_name);
  }

  return paths;
}
}  // namespace metrics
}  // namespace utils

#endif  // PTI_SAMPLES_UTILS_METRIC_UTILS_H_
