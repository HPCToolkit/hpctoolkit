// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "../../hpcprof/stdshim/filesystem.hpp"
#include "elf-hash.h"

#include <gtest/gtest.h>

static hpctoolkit::stdshim::filesystem::path example_data;
static const char* example_hash =
    "a273798061b3241887296c5b990fed3d"; // pragma: allowlist secret

TEST(ElfHashTest, Hash) {
  char* hash = elf_hash(example_data.c_str());
  ASSERT_NE(hash, nullptr);
  EXPECT_STREQ(hash, example_hash);
  free(hash);
}

auto main(int argc, char** argv) -> int {
  testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "Incorrect number of arguments\n";
    return 2;
  }
  example_data = argv[1];
  return RUN_ALL_TESTS();
}
