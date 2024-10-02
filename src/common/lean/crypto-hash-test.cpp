// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "crypto-hash.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

static const std::array<unsigned char, 128> data = {
    0x39, 0x80, 0x98, 0x07, 0x06, 0xbf, 0x66, 0x2b, 0x03, 0x0e, 0x5b, 0xd0, 0x9f,
    0x69, 0x44, 0xb3, 0x61, 0x08, 0x5d, 0x07, 0xec, 0x31, 0x6e, 0x81, 0xd2, 0x8f,
    0x81, 0xd3, 0x8e, 0x3c, 0x1d, 0xdb, 0x18, 0x24, 0x7c, 0xe7, 0x1e, 0xbb, 0x1a,
    0xca, 0x97, 0xa8, 0x14, 0x6c, 0xd7, 0x6c, 0xd8, 0xef, 0xa3, 0x01, 0xec, 0xdd,
    0xeb, 0xa7, 0x53, 0x6b, 0x25, 0x24, 0x02, 0xc7, 0x20, 0x2d, 0x33, 0xdb, 0x33,
    0x63, 0xf6, 0x32, 0xa2, 0x15, 0x83, 0x88, 0x62, 0xb1, 0x06, 0x31, 0xeb, 0x37,
    0x48, 0xc5, 0x29, 0x8e, 0x11, 0xb4, 0x66, 0x82, 0xc9, 0x59, 0x2c, 0x3b, 0x1d,
    0xfc, 0xd3, 0xde, 0xcf, 0xcf, 0x7b, 0x49, 0x01, 0x7f, 0xb3, 0x10, 0x7d, 0x6e,
    0x21, 0x2b, 0x89, 0xb4, 0x59, 0x0a, 0x2f, 0x66, 0xaf, 0x31, 0xad, 0x42, 0x39,
    0xf1, 0x92, 0xda, 0x52, 0xb8, 0xcd, 0x15, 0xee, 0x10, 0x65, 0x5b,
};

static const std::array<unsigned char, CRYPTO_HASH_LENGTH> known_hash = {
    0x12, 0x9e, 0xd6, 0x35, 0xf1, 0xe8, 0xc9, 0xd6,
    0x39, 0x24, 0x83, 0x25, 0x53, 0x3f, 0x24, 0xb3,
};

static const char* known_hashstr =
    "129ed635f1e8c9d639248325533f24b3"; // pragma: allowlist secret

TEST(CryptoHashTest, Hash) {
  std::array<unsigned char, CRYPTO_HASH_LENGTH> hash;
  int error = crypto_compute_hash(data.data(), data.size(), hash.data(), hash.size());
  ASSERT_FALSE(error);
  EXPECT_EQ(hash, known_hash);
}

TEST(CryptoHashTest, ShortHashBuffer) {
  std::array<unsigned char, CRYPTO_HASH_LENGTH - 1> short_hash;
  int error = crypto_compute_hash(data.data(), data.size(), short_hash.data(),
                                  short_hash.size());
  EXPECT_TRUE(error);
}

TEST(CryptoHashTest, LongHashBuffer) {
  std::array<unsigned char, CRYPTO_HASH_LENGTH + 10> long_hash;
  long_hash.fill(0xff);
  int error =
      crypto_compute_hash(data.data(), data.size(), long_hash.data(), long_hash.size());
  ASSERT_FALSE(error);
  std::array<unsigned char, long_hash.size()> extended_known_hash;
  extended_known_hash.fill(0);
  std::copy(known_hash.begin(), known_hash.end(), extended_known_hash.begin());
  EXPECT_EQ(long_hash, extended_known_hash);
}

TEST(CryptoHashTest, Hexstring) {
  std::array<char, CRYPTO_HASH_STRING_LENGTH> hashstr;
  hashstr.fill(0x7f);
  int error = crypto_compute_hash_string(data.data(), data.size(), hashstr.data(),
                                         hashstr.size());
  ASSERT_FALSE(error);
  ASSERT_EQ(hashstr.back(), 0);
  EXPECT_STREQ(hashstr.data(), known_hashstr);
}

TEST(CryptoHashTest, ShortStringBuffer) {
  std::array<char, CRYPTO_HASH_STRING_LENGTH - 1> hashstr;
  int error = crypto_compute_hash_string(data.data(), data.size(), hashstr.data(),
                                         hashstr.size());
  EXPECT_TRUE(error);
}

TEST(CryptoHashTest, LongStringBuffer) {
  std::array<char, CRYPTO_HASH_STRING_LENGTH + 10> hashstr;
  hashstr.fill(0x7f);
  int error = crypto_compute_hash_string(data.data(), data.size(), hashstr.data(),
                                         hashstr.size());
  ASSERT_FALSE(error);
  ASSERT_EQ(hashstr[CRYPTO_HASH_STRING_LENGTH - 1], 0);
  EXPECT_STREQ(hashstr.data(), known_hashstr);
}
