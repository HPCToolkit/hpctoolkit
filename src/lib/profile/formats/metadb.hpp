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
// Copyright ((c)) 2002-2023, Rice University
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

#ifndef HPCTOOLKIT_PROFILE_FORMATS_MetaDB_H
#define HPCTOOLKIT_PROFILE_FORMATS_MetaDB_H

#include "core.hpp"

#include "../../prof-lean/formats/metadb.h"

namespace hpctoolkit::formats {

template<>
struct DataTraits<fmt_metadb_fHdr_t> : ConstSizeDataTraits<fmt_metadb_fHdr_t, FMT_METADB_SZ_FHdr, 8> {
  ser_type serialize(const fmt_metadb_fHdr_t& data) noexcept {
    ser_type result;
    fmt_metadb_fHdr_write(result.data(), &data);
    return result;
  }
};

template<>
struct DataTraits<fmt_metadb_generalSHdr_t> : ConstSizeDataTraits<fmt_metadb_generalSHdr_t, FMT_METADB_SZ_GeneralSHdr, 8> {
  ser_type serialize(const fmt_metadb_generalSHdr_t& data) noexcept {
    ser_type result;
    fmt_metadb_generalSHdr_write(result.data(), &data);
    return result;
  }
  using parent_type = fmt_metadb_fHdr_t;
  void update_parent(parent_type& parent, std::uint_fast64_t offset, std::uint_fast64_t size) {
    parent.pGeneral = offset;
    parent.szGeneral = size - offset;
  }
};

template<>
struct DataTraits<fmt_metadb_idNamesSHdr_t> : ConstSizeDataTraits<fmt_metadb_idNamesSHdr_t, FMT_METADB_SZ_IdNamesSHdr, 8> {
  ser_type serialize(const fmt_metadb_idNamesSHdr_t& data) noexcept {
    ser_type result;
    fmt_metadb_idNamesSHdr_write(result.data(), &data);
    return result;
  }
  using parent_type = fmt_metadb_fHdr_t;
  void update_parent(parent_type& parent, std::uint_fast64_t offset, std::uint_fast64_t size) {
    parent.pIdNames = offset;
    parent.szIdNames = size - offset;
  }
};

template<>
struct DataTraits<fmt_metadb_metricsSHdr_t> : ConstSizeDataTraits<fmt_metadb_metricsSHdr_t, FMT_METADB_SZ_MetricsSHdr, 8> {
  ser_type serialize(const fmt_metadb_metricsSHdr_t& data) noexcept {
    ser_type result;
    fmt_metadb_metricsSHdr_write(result.data(), &data);
    return result;
  }
  using parent_type = fmt_metadb_fHdr_t;
  void update_parent(parent_type& parent, std::uint_fast64_t offset, std::uint_fast64_t size) {
    parent.pMetrics = offset;
    parent.szMetrics = size - offset;
  }
};

template<>
struct DataTraits<fmt_metadb_metricDesc_t> : ConstSizeDataTraits<fmt_metadb_metricDesc_t, FMT_METADB_SZ_MetricDesc, 8> {
  ser_type serialize(const fmt_metadb_metricDesc_t& data) noexcept {
    ser_type result;
    fmt_metadb_metricDesc_write(result.data(), &data);
    return result;
  }
};

template<>
struct DataTraits<fmt_metadb_propScopeInst_t> : ConstSizeDataTraits<fmt_metadb_propScopeInst_t, FMT_METADB_SZ_PropScopeInst, 8> {
  ser_type serialize(const fmt_metadb_propScopeInst_t& data) noexcept {
    ser_type result;
    fmt_metadb_propScopeInst_write(result.data(), &data);
    return result;
  }
};

template<>
struct DataTraits<fmt_metadb_summaryStat_t> : ConstSizeDataTraits<fmt_metadb_summaryStat_t, FMT_METADB_SZ_SummaryStat, 8> {
  ser_type serialize(const fmt_metadb_summaryStat_t& data) noexcept {
    ser_type result;
    fmt_metadb_summaryStat_write(result.data(), &data);
    return result;
  }
};

template<>
struct DataTraits<fmt_metadb_propScope_t> : ConstSizeDataTraits<fmt_metadb_propScope_t, FMT_METADB_SZ_PropScope, 8> {
  ser_type serialize(const fmt_metadb_propScope_t& data) noexcept {
    ser_type result;
    fmt_metadb_propScope_write(result.data(), &data);
    return result;
  }
};

template<>
struct DataTraits<fmt_metadb_modulesSHdr_t> : ConstSizeDataTraits<fmt_metadb_modulesSHdr_t, FMT_METADB_SZ_ModulesSHdr, 8> {
  ser_type serialize(const fmt_metadb_modulesSHdr_t& data) noexcept {
    ser_type result;
    fmt_metadb_modulesSHdr_write(result.data(), &data);
    return result;
  }
  using parent_type = fmt_metadb_fHdr_t;
  void update_parent(parent_type& parent, std::uint_fast64_t offset, std::uint_fast64_t size) {
    parent.pModules = offset;
    parent.szModules = size - offset;
  }
};

template<>
struct DataTraits<fmt_metadb_moduleSpec_t> : ConstSizeDataTraits<fmt_metadb_moduleSpec_t, FMT_METADB_SZ_ModuleSpec, 8> {
  ser_type serialize(const fmt_metadb_moduleSpec_t& data) noexcept {
    ser_type result;
    fmt_metadb_moduleSpec_write(result.data(), &data);
    return result;
  }
};

template<>
struct DataTraits<fmt_metadb_filesSHdr_t> : ConstSizeDataTraits<fmt_metadb_filesSHdr_t, FMT_METADB_SZ_FilesSHdr, 8> {
  ser_type serialize(const fmt_metadb_filesSHdr_t& data) noexcept {
    ser_type result;
    fmt_metadb_filesSHdr_write(result.data(), &data);
    return result;
  }
  using parent_type = fmt_metadb_fHdr_t;
  void update_parent(parent_type& parent, std::uint_fast64_t offset, std::uint_fast64_t size) {
    parent.pFiles = offset;
    parent.szFiles = size - offset;
  }
};

template<>
struct DataTraits<fmt_metadb_fileSpec_t> : ConstSizeDataTraits<fmt_metadb_fileSpec_t, FMT_METADB_SZ_FileSpec, 8> {
  ser_type serialize(const fmt_metadb_fileSpec_t& data) noexcept {
    ser_type result;
    fmt_metadb_fileSpec_write(result.data(), &data);
    return result;
  }
};

template<>
struct DataTraits<fmt_metadb_functionsSHdr_t> : ConstSizeDataTraits<fmt_metadb_functionsSHdr_t, FMT_METADB_SZ_FunctionsSHdr, 8> {
  ser_type serialize(const fmt_metadb_functionsSHdr_t& data) noexcept {
    ser_type result;
    fmt_metadb_functionsSHdr_write(result.data(), &data);
    return result;
  }
  using parent_type = fmt_metadb_fHdr_t;
  void update_parent(parent_type& parent, std::uint_fast64_t offset, std::uint_fast64_t size) {
    parent.pFunctions = offset;
    parent.szFunctions = size - offset;
  }
};

template<>
struct DataTraits<fmt_metadb_functionSpec_t> : ConstSizeDataTraits<fmt_metadb_functionSpec_t, FMT_METADB_SZ_FunctionSpec, 8> {
  ser_type serialize(const fmt_metadb_functionSpec_t& data) noexcept {
    ser_type result;
    fmt_metadb_functionSpec_write(result.data(), &data);
    return result;
  }
};

template<>
struct DataTraits<fmt_metadb_contextsSHdr_t> : ConstSizeDataTraits<fmt_metadb_contextsSHdr_t, FMT_METADB_SZ_ContextsSHdr, 8> {
  ser_type serialize(const fmt_metadb_contextsSHdr_t& data) noexcept {
    ser_type result;
    fmt_metadb_contextsSHdr_write(result.data(), &data);
    return result;
  }
  using parent_type = fmt_metadb_fHdr_t;
  void update_parent(parent_type& parent, std::uint_fast64_t offset, std::uint_fast64_t size) {
    parent.pContext = offset;
    parent.szContext = size - offset;
  }
};

template<>
struct DataTraits<fmt_metadb_entryPoint_t> : ConstSizeDataTraits<fmt_metadb_entryPoint_t, FMT_METADB_SZ_EntryPoint, 8> {
  ser_type serialize(const fmt_metadb_entryPoint_t& data) noexcept {
    ser_type result;
    fmt_metadb_entryPoint_write(result.data(), &data);
    return result;
  }
};

template<>
struct DataTraits<fmt_metadb_context_t> {
  static inline constexpr std::uint8_t alignment = 8;
  std::vector<char> serialize(const fmt_metadb_context_t& data) noexcept {
    std::vector<char> result(FMT_METADB_MAXSZ_Context);
    result.resize(fmt_metadb_context_write(result.data(), &data));
    return result;
  }
};

}  // namespace hpctoolkit::formats

#endif  // HPCTOOLKIT_PROFILE_FORMATS_MetaDB_H
