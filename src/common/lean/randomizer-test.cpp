// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "randomizer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <numeric>

// test that the random levels for skip list node heights have the proper
// distribution between 1 .. max_height, where the probability of 2^h is
// half that of 2^(h-1), for h in [2 .. max_height]
TEST(RandomizerTest, Distribution) {
  constexpr size_t samples = 1024ULL * 1024;
  constexpr size_t degrees = 10;
  constexpr double critical_chi = 25.19; // p = 1/2000

  std::array<double, degrees + 1> bins;
  bins.fill(0);
  for (size_t i = 0; i < samples; ++i) {
    int l = random_level(bins.size());
    ASSERT_GE(l, 1);
    ASSERT_LE(l, bins.size());
    ++bins[l - 1];
  }

  std::array<double, bins.size()> expected;
  {
    expected[0] = 1ULL << bins.size();
    for (size_t l = 1; l < expected.size(); ++l)
      expected[l] = expected[l - 1] / 2;
    double factor =
        (double)samples / std::accumulate(expected.begin(), expected.end(), 0.0);
    for (double& e : expected)
      e *= factor;
  }

  std::vector<double> chis;
  std::transform(bins.begin(), bins.end(), expected.begin(), std::back_inserter(chis),
                 [](double b, double e) { return b * b / e; });
  auto chi = std::accumulate(chis.begin(), chis.end(), 0.0) - samples;
  ASSERT_LT(chi, critical_chi);
}
