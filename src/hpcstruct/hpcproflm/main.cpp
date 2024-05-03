// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

//****************************************************************************
// system include files
//****************************************************************************

#include <iostream>
#include <exception>
#include <string>
#include <unordered_set>



//****************************************************************************
// local include files
//****************************************************************************

#include "Args.hpp"

#include "../../hpcprof/stdshim/filesystem.hpp"
#include "../../common/lean/hpcrun-fmt.h"
#include "../../common/diagnostics.h"


namespace fs = hpctoolkit::stdshim::filesystem;


//****************************************************************************
// macros
//****************************************************************************

#define GPUBIN_SUFFIX_LEN 6 // strlen("gpubin")


//****************************************************************************
// local data
//****************************************************************************

static const char *filter_strings[] = {
  "logical/",
  "kernel_symbols/",
  0
};



//****************************************************************************
// internal operations
//****************************************************************************

static bool
skipLoadMapEntry(loadmap_entry_t* x)
{
  // if this load map entry doesn't need to be analyzed
  if (!(x->flags & LOADMAP_ENTRY_ANALYZE)) return true;

  // ignore empty names
  if (x->name == NULL) return true;

  bool result = false;
  for (int i = 0; filter_strings[i]; i++) {
    result |= strstr(x->name, filter_strings[i]) != 0;
  }

  return result;
}


// for any gpubin, erase any kernel name hash following "gpubin" the name
static std::string
getLoadModuleName(loadmap_entry_t* x)
{
  std::string name(x->name);
  size_t pos = name.find("gpubin.");
  if (pos != std::string::npos) {
    // erase kernel hash suffix from the gpubin name
    name.erase(pos+GPUBIN_SUFFIX_LEN);
  }

  return name;
}


static bool
readFooter(FILE* fs, hpcrun_fmt_footer_t &footer)
{
  fseek(fs, 0, SEEK_END);
  size_t footer_position = ftell(fs) - SF_footer_SIZE;
  fseek(fs, footer_position, SEEK_SET);

  return hpcrun_fmt_footer_fread(&footer, fs) == HPCFMT_OK;
}


static bool
readLoadmap(FILE *infs, hpcrun_fmt_footer_t &footer, std::unordered_set<std::string> &loadModules)
{
  fseek(infs, footer.loadmap_start, SEEK_SET);

  loadmap_t loadmap_tbl;
  int ret = hpcrun_fmt_loadmap_fread(&loadmap_tbl, infs, malloc);
  if (ret != HPCFMT_OK || (uint64_t)ftell(infs) != footer.loadmap_end) {
    return false;
  }

  for (uint32_t i = 0; i < loadmap_tbl.len; i++) {
    loadmap_entry_t* x = &loadmap_tbl.lst[i];
    if (skipLoadMapEntry(x)) continue;
    std::string name = getLoadModuleName(x);
    loadModules.insert(name);
  }

  return true;
}


static void
processProfile(const fs::path &path, std::unordered_set<std::string> &loadModules)
{
   std::string filename = path;
   const char *fnm = filename.c_str();

   FILE* fs = hpcio_fopen_r(fnm);
   if (fs) {
    hpcrun_fmt_footer_t footer;
    bool status = readFooter(fs, footer);
    if (status) {
      status = readLoadmap(fs, footer, loadModules);
    }
    DIAG_WMsgIf(status == false, "unable to extract loadmap from profile " << filename);
    fclose(fs);
   }
}


static int
processMeasurementsDirectory(Args &args)
{
  int status = 0;
  const fs::path path(args.measurements_directory);
  std::string pathname = path;

  if (!fs::is_directory(path)) {
    DIAG_EMsg(pathname << " is not a directory");
    status = 1;
  } else {
    std::vector<fs::path> hpcrunFiles;
    for (auto const& dir_entry : fs::directory_iterator(path)) {
      if (dir_entry.path().extension() == ".hpcrun") {
        hpcrunFiles.push_back(dir_entry.path());
      }
    }
    if (hpcrunFiles.begin() == hpcrunFiles.end()) {
      DIAG_EMsg("directory " << pathname <<  " does not contain any HPCToolkit profiles");
      status = 1;
    } else {
      std::unordered_set<std::string> loadModules;
      #pragma omp parallel shared(loadModules)
      {
        std::unordered_set<std::string> privateLoadModules;
        #pragma omp for
        for (size_t i = 0; i < hpcrunFiles.size(); i++) {
          processProfile(hpcrunFiles[i], privateLoadModules);
        }
        #pragma omp critical
        loadModules.merge(std::move(privateLoadModules));
      }
      for (const auto& lm: loadModules) {
        std::cout << lm << "\n";
      }
    }
  }

  return status;
}



//****************************************************************************
// interface operations
//****************************************************************************

int
main(int argc, char* const* argv)
{
  int ret;

  try {
    Args args(argc, argv);  // exits if error on command line
    ret = processMeasurementsDirectory(args);
  }
  catch (const Diagnostics::Exception& x) {
    DIAG_EMsg(x.message());
    ret = 1;
  }
  catch (...) {
    DIAG_EMsg("unknown exception encountered");
    ret = 1;
  }

  return ret;
}
