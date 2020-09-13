//==============================================================
// Copyright © 2019 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_UTILS_UTILS_H_
#define PTI_SAMPLES_UTILS_UTILS_H_

#if defined(_WIN32)
#include <windows.h>
#elif defined(__gnu_linux__)
#include <unistd.h>
#endif

#include <assert.h>
#include <stdint.h>

#include <fstream>
#include <string>
#include <vector>

#define MAX_STR_SIZE 1024

#define NSEC_IN_USEC 1000
#define NSEC_IN_MSEC 1000000
#define NSEC_IN_SEC  1000000000

namespace utils {

struct Comparator {
  template<typename T>
  bool operator()(const T& left, const T& right) {
    if (left.second != right.second) {
      return left.second > right.second;
    }
    return left.first > right.first;
  }
};

#if defined(__gnu_linux__)
uint64_t ConvertClockMonotonicToRaw(uint64_t clock_monotonic) {
  int status = 0;

  timespec monotonic_time;
  status = clock_gettime(CLOCK_MONOTONIC, &monotonic_time);
  assert(status == 0);

  timespec raw_time;
  status = clock_gettime(CLOCK_MONOTONIC_RAW, &raw_time);
  assert(status == 0);

  uint64_t raw = raw_time.tv_nsec + NSEC_IN_SEC * raw_time.tv_sec;
  uint64_t monotonic = monotonic_time.tv_nsec +
    NSEC_IN_SEC * monotonic_time.tv_sec;
  if (raw > monotonic) {
    return clock_monotonic + (raw - monotonic);
  } else {
    return clock_monotonic - (monotonic - raw);
  }
}
#endif

inline std::string GetExecutablePath() {
  char buffer[MAX_STR_SIZE] = { 0 };
#if defined(_WIN32)
  DWORD status = GetModuleFileNameA(nullptr, buffer, MAX_STR_SIZE);
  assert(status > 0);
#elif defined(__gnu_linux__)
  ssize_t status = readlink("/proc/self/exe", buffer, MAX_STR_SIZE);
  assert(status > 0);
#else
#error not supported
#endif
  std::string path(buffer);
  return path.substr(0, path.find_last_of("/\\") + 1);
}

inline std::vector<uint8_t> LoadBinaryFile(const std::string& path) {
  std::vector<uint8_t> binary;
  std::ifstream stream(path, std::ios::in | std::ios::binary);
  if (!stream.good()) {
    return binary;
  }

  size_t size = 0;
  stream.seekg(0, std::ifstream::end);
  size = static_cast<size_t>(stream.tellg());
  stream.seekg(0, std::ifstream::beg);
  if (size == 0) {
    return binary;
  }

  binary.resize(size);
  stream.read(reinterpret_cast<char *>(binary.data()), size);
  return binary;
}

inline void SetEnv(const std::string& str) {
  int status = 0;
#if defined(_WIN32)
  status = _putenv(str.c_str());
#elif defined(__gnu_linux__)
  status = putenv(const_cast<char*>(str.c_str()));
#else
#error not supported
#endif
  assert(status == 0);
}

inline const char* GetEnv(const char* name) {
  return getenv(name);
}

} // namespace utils

#endif // PTI_SAMPLES_UTILS_UTILS_H_