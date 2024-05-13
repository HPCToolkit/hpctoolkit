// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#ifndef HPCTOOLKIT_PROFILE_FORMATS_MetaDB_H
#define HPCTOOLKIT_PROFILE_FORMATS_MetaDB_H

#include "core.hpp"

#include "../../common/lean/formats/metadb.h"

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
