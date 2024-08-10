// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "exclude.h"

#include <gtest/gtest.h>

TEST(SampleSourcesTest, Exclude) {
  EXPECT_FALSE(is_event_to_exclude("re"));
  EXPECT_TRUE(is_event_to_exclude("realtime"));
  EXPECT_FALSE(is_event_to_exclude("ret"));
  EXPECT_TRUE(is_event_to_exclude("retcnt"));
  EXPECT_FALSE(is_event_to_exclude(""));
  EXPECT_FALSE(is_event_to_exclude("//"));
}
