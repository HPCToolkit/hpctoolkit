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

#ifndef PTI_SAMPLES_UTILS_SHARED_LIBRARY_H_
#define PTI_SAMPLES_UTILS_SHARED_LIBRARY_H_

#include <assert.h>

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__gnu_linux__) || defined(__APPLE__)
#include <cerrno>
#include <dlfcn.h>
#endif

#include <string>
#include <vector>

class SharedLibrary {
 public:
  static SharedLibrary* Create(const std::string& name) {
#if defined(_WIN32)
    HMODULE handle = nullptr;
    handle = LoadLibraryA(name.c_str());
#elif defined(__gnu_linux__) || defined(__APPLE__)
    void* handle = nullptr;
    handle = dlopen(name.c_str(), RTLD_NOW);
#endif
    if (handle != nullptr) {
      return new SharedLibrary(handle);
    }
    return nullptr;
  }

  ~SharedLibrary() {
#if defined(_WIN32)
    BOOL completed = FreeLibrary(handle_);
    assert(completed == TRUE);
#elif defined(__gnu_linux__) || defined(__APPLE__)
    int completed = dlclose(handle_);
    assert(completed == 0);
#endif
  }

  template<typename T> T GetSym(const char* name) {
    void* sym = nullptr;
#if defined(_WIN32)
    sym = GetProcAddress(handle_, name);
#elif defined(__gnu_linux__) || defined(__APPLE__)
    sym = dlsym(handle_, name);
#endif
    return reinterpret_cast<T>(sym);
  }

 private:
#if defined(_WIN32)
  SharedLibrary(HMODULE handle) : handle_(handle) {}
#elif defined(__gnu_linux__) || defined(__APPLE__)
  SharedLibrary(void* handle) : handle_(handle) {}
#endif

#if defined(_WIN32)
  HMODULE handle_ = nullptr;
#elif defined(__gnu_linux__) || defined(__APPLE__)
  void* handle_ = nullptr;
#endif
};

#endif // PTI_SAMPLES_UTILS_SHARED_LIBRARY_H_
