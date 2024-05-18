// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#include "util/vgannotations.hpp"

#include "metric.hpp"

#include "context.hpp"
#include "attributes.hpp"

#include <forward_list>
#include <stack>
#include <thread>
#include <ostream>

using namespace hpctoolkit;

std::ostream& hpctoolkit::operator<<(std::ostream& os, Statistic::combination_t c) {
  switch(c) {
  case Statistic::combination_t::sum: return os << "sum";
  case Statistic::combination_t::min: return os << "min";
  case Statistic::combination_t::max: return os << "max";
  }
  std::abort();
}

ExtraStatistic::ExtraStatistic(Settings s)
  : u_settings(std::move(s)) {
  const auto& fmt = u_settings().format;
  size_t pos = 0;
  bool found = false;
  while((pos = fmt.find('%', pos)) != std::string::npos) {
    if(fmt[pos+1] == '%') pos += 2;
    else {
      if(found)
        util::log::fatal{} << "Multiple arguments in format string for ExtraStatistic: " << fmt;
      found = true;
      pos += 1;
    }
  }
}

Statistic::Statistic(std::string suff, bool showp, Expression form, bool showBD)
  : m_suffix(std::move(suff)), m_showPerc(showp), m_formula(std::move(form)),
    m_visibleByDefault(showBD) {};

Metric::Metric(Metric&& o)
  : userdata(std::move(o.userdata), std::cref(*this)),
    u_settings(std::move(o.u_settings)),
    m_thawed_stats(std::move(o.m_thawed_stats)),
    m_thawed_sumPartial(o.m_thawed_sumPartial),
    m_frozen(o.m_frozen.load(std::memory_order_relaxed)),
    m_partials(std::move(o.m_partials)),
    m_stats(std::move(o.m_stats)) {};

Metric::Metric(ud_t::struct_t& rs, Settings s)
  : userdata(rs, std::cref(*this)), u_settings(std::move(s)),
    m_thawed_sumPartial(std::numeric_limits<std::size_t>::max()),
    m_frozen(false) {};

std::unique_lock<std::mutex> Metric::StatsAccess::synchronize() {
  if(m.m_frozen.load(std::memory_order_acquire)) {
    ANNOTATE_HAPPENS_AFTER(&m.m_frozen);
    return {};
  }
  std::unique_lock<std::mutex> l{m.m_frozen_lock};
  if(m.m_frozen.load(std::memory_order_relaxed)) {
    ANNOTATE_HAPPENS_AFTER(&m.m_frozen);
    return {};
  }
  return l;
}

void Metric::StatsAccess::requestStatistics(Statistics ss) {
  // If the Metric is invisible, we don't actually need to do anything
  if(m.visibility() == Settings::visibility_t::invisible) {
    assert(!ss.sum && !ss.mean && !ss.min && !ss.max && !ss.stddev && !ss.cfvar
           && "Attempt to request Statistics from an invisible Metric!");
    return;
  }

  auto l = synchronize();
  if(!l) {
    // If we're already frozen, this operation must be idempotent.
    assert((!ss.sum    || m.m_thawed_stats.sum)    && "Attempt to request :Sum from a frozen Metric!");
    assert((!ss.mean   || m.m_thawed_stats.mean)   && "Attempt to request :Mean from a frozen Metric!");
    assert((!ss.min    || m.m_thawed_stats.min)    && "Attempt to request :Min from a frozen Metric!");
    assert((!ss.max    || m.m_thawed_stats.max)    && "Attempt to request :Max from a frozen Metric!");
    assert((!ss.stddev || m.m_thawed_stats.stddev) && "Attempt to request :StdDev from a frozen Metric!");
    assert((!ss.cfvar  || m.m_thawed_stats.cfvar)  && "Attempt to request :CfVar from a frozen Metric!");
    return;
  }
  m.m_thawed_stats.sum = m.m_thawed_stats.sum || ss.sum;
  m.m_thawed_stats.mean = m.m_thawed_stats.mean || ss.mean;
  m.m_thawed_stats.min = m.m_thawed_stats.min || ss.min;
  m.m_thawed_stats.max = m.m_thawed_stats.max || ss.max;
  m.m_thawed_stats.stddev = m.m_thawed_stats.stddev || ss.stddev;
  m.m_thawed_stats.cfvar = m.m_thawed_stats.cfvar || ss.cfvar;
}

std::size_t Metric::StatsAccess::requestSumPartial() {
  auto l = synchronize();
  if(m.m_thawed_sumPartial != std::numeric_limits<std::size_t>::max())
    return m.m_thawed_sumPartial;
  assert(l && "Unable to satisfy :Sum Partial request on a frozen Metric!");

  m.m_thawed_sumPartial = m.m_partials.size();
  m.m_partials.push_back({Expression::variable, Statistic::combination_t::sum,
                          m.m_thawed_sumPartial});
  return m.m_thawed_sumPartial;
}

bool Metric::freeze() {
  if(m_frozen.load(std::memory_order_acquire)) {
    ANNOTATE_HAPPENS_AFTER(&m_frozen);
    return false;
  }
  std::unique_lock<std::mutex> l{m_frozen_lock};
  if(m_frozen.load(std::memory_order_relaxed)) {
    ANNOTATE_HAPPENS_AFTER(&m_frozen);
    return false;
  }

  const Statistics& ss = m_thawed_stats;

  size_t cntIdx = -1;
  if(ss.mean || ss.stddev || ss.cfvar) {
    cntIdx = m_partials.size();
    m_partials.push_back({1, Statistic::combination_t::sum, cntIdx});
  }
  if(m_thawed_sumPartial == std::numeric_limits<std::size_t>::max()) {
    m_thawed_sumPartial = m_partials.size();
    m_partials.push_back({Expression::variable, Statistic::combination_t::sum,
                          m_thawed_sumPartial});
  }
  size_t x2Idx = -1;
  if(ss.stddev || ss.cfvar) {
    x2Idx = m_partials.size();
    m_partials.push_back({{Expression::Kind::op_pow, {Expression::variable, 2}},
                          Statistic::combination_t::sum, x2Idx});
  }
  size_t minIdx = -1;
  if(ss.min) {
    minIdx = m_partials.size();
    m_partials.push_back({Expression::variable, Statistic::combination_t::min,
                          minIdx});
  }
  size_t maxIdx = -1;
  if(ss.max) {
    maxIdx = m_partials.size();
    m_partials.push_back({Expression::variable, Statistic::combination_t::max,
                          maxIdx});
  }

  if(ss.sum)
    m_stats.push_back({"Sum", true, {Expression::variable, m_thawed_sumPartial},
                       u_settings().visibility == Settings::visibility_t::shownByDefault});
  if(ss.mean)
    m_stats.push_back({"Mean", false,
      {Expression::Kind::op_div, { {Expression::variable, m_thawed_sumPartial},
                                   {Expression::variable, cntIdx}}},
      u_settings().visibility == Settings::visibility_t::shownByDefault});
  if(ss.stddev)
    m_stats.push_back({"StdDev", false,
      {Expression::Kind::op_sqrt, {{Expression::Kind::op_sub, {
        {Expression::Kind::op_div, { {Expression::variable, x2Idx},
                                     {Expression::variable, cntIdx}}},
        {Expression::Kind::op_pow, {
          {Expression::Kind::op_div, { {Expression::variable, m_thawed_sumPartial},
                                       {Expression::variable, cntIdx}}},
          2,
        }},
      }}}},
      u_settings().visibility == Settings::visibility_t::shownByDefault});
  if(ss.cfvar)
    m_stats.push_back({"CfVar", false,
      {Expression::Kind::op_div, {
        {Expression::Kind::op_sqrt, {{Expression::Kind::op_sub, {
          {Expression::Kind::op_div, { {Expression::variable, x2Idx},
                                       {Expression::variable, cntIdx}}},
          {Expression::Kind::op_pow, {
            {Expression::Kind::op_div, { {Expression::variable, m_thawed_sumPartial},
                                         {Expression::variable, cntIdx}}},
            2,
          }},
        }}}},
        {Expression::Kind::op_div, { {Expression::variable, m_thawed_sumPartial},
                                     {Expression::variable, cntIdx}}},
      }},
      u_settings().visibility == Settings::visibility_t::shownByDefault});
  if(ss.min)
    m_stats.push_back({"Min", false, {Expression::variable, minIdx},
                       u_settings().visibility == Settings::visibility_t::shownByDefault});
  if(ss.max)
    m_stats.push_back({"Max", false, {Expression::variable, maxIdx},
                       u_settings().visibility == Settings::visibility_t::shownByDefault});

  ANNOTATE_HAPPENS_BEFORE(&m_frozen);
  m_frozen.store(true, std::memory_order_release);
  return true;
}

const std::vector<StatisticPartial>& Metric::partials() const noexcept {
  assert(m_frozen.load(std::memory_order_relaxed) && "Attempt to access a Metric's Partials before a freeze!");
  return m_partials;
}
const std::vector<Statistic>& Metric::statistics() const noexcept {
  assert(m_frozen.load(std::memory_order_relaxed) && "Attempt to access a Metric's Statistics before a freeze!");
  return m_stats;
}

util::optional_ref<const MetricAccumulator> Metric::getFor(const PerThreadTemporary& t, const Context& c) const noexcept {
  auto cd = t.c_data.find(c);
  if(!cd) return std::nullopt;
  return cd->find(*this);
}

std::size_t std::hash<Metric::Settings>::operator()(const Metric::Settings &s) const noexcept {
  const auto h1 = std::hash<std::string>{}(s.name);
  const auto h2 = std::hash<std::string>{}(s.description);
  return h1 ^ ((h2 << 1) | (h2 >> (-1 + 8 * sizeof h2)));
}
