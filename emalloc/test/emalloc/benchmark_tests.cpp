// /////////////////////////////////////////////////////////////////////////////
/// @file benchmark_tests.cpp
/// @brief ./build/emalloc/test/RunAllTests -g BenchmarkTests
///
/// @par  Plataform Target: Tests
///
/// @copyright (C) 2025 Mario Luzeiro All rights reserved.
/// @author Mario Luzeiro <mluzeiro@ua.pt>
///
/// @par  License: Distributed under the 3-Clause BSD License. See accompanying
/// file LICENSE or a copy at https://opensource.org/licenses/BSD-3-Clause
/// SPDX-License-Identifier: BSD-3-Clause
///
// /////////////////////////////////////////////////////////////////////////////

// Includes
// /////////////////////////////////////////////////////////////////////////////
#include <CppUTest/TestHarness.h>
#include <CppUTest/UtestMacros.h>
#include <emalloc/emalloc.h>
#include <cstdio>
#include <random>
#include <vector>

extern FILE* s_benchmark_tests_log_file;

// clang-format off
// NOLINTBEGIN
TEST_GROUP(BenchmarkTests){
  static const uint32_t EXT_RAM_SIZE = 512*1024;
  // This allows the teorically maximum nodes to be allocated
  static const uint32_t MAX_NODES = EXT_RAM_SIZE / 16;

  uint64_t nodes_poll[MAX_NODES];
  uint8_t external_ram[EXT_RAM_SIZE];

  sEMALLOC_ctx emalloc_ctx;

  uint32_t requested_size = 0;
  uint32_t remain_size = EXT_RAM_SIZE;

  void setup() {
    if (!s_benchmark_tests_log_file) {
      s_benchmark_tests_log_file = fopen("BenchmarkTests.log", "w");

      fprintf( s_benchmark_tests_log_file, "EXT_RAM_SIZE:%u, MAX_NODES:%u\n",
        EXT_RAM_SIZE, MAX_NODES);

      fprintf( s_benchmark_tests_log_file,
        "alloc/free\n");
      fprintf( s_benchmark_tests_log_file,
        "TestName\tn_ifs\tn_loops\trd_wr,\tn_ifs\tn_loops\trd_wr\n");
    }

    // Initialize buffers
    memset(nodes_poll, 0, sizeof(nodes_poll));
    memset(external_ram, 0, sizeof(external_ram));

    sEMALLOC_cfg emalloc_configuration;

    emalloc_configuration.nodes_poll = nodes_poll;
    emalloc_configuration.nodes_poll_length = MAX_NODES;
    emalloc_configuration.external_memory_size_bytes = EXT_RAM_SIZE;

    emalloc_reset_statistics();

    CHECK_EQUAL(EMALLOC_OK,
      emalloc_init(&emalloc_ctx,
        &emalloc_configuration));
  }

  void teardown() {
    sEMALLOC_statistics stats;
    emalloc_get_statistics(&stats);

    fprintf( s_benchmark_tests_log_file,
      "\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n",
      stats.alloc.n_calls?(stats.alloc.n_ifs / stats.alloc.n_calls):0,
      stats.alloc.n_calls?(stats.alloc.n_loops / stats.alloc.n_calls):0,
      stats.alloc.n_calls?(stats.alloc.n_nodes_rd_wr / stats.alloc.n_calls):0,
      stats.free.n_calls?(stats.free.n_ifs / stats.free.n_calls):0,
      stats.free.n_calls?(stats.free.n_loops/ stats.free.n_calls):0,
      stats.free.n_calls?(stats.free.n_nodes_rd_wr / stats.free.n_calls):0);
  }
};
// NOLINTEND
// clang-format on

// From: "TLSF: A new dynamic memory allocator for real-time systems"
// "Test-1 malloc/free worst case for First-Fit. [...]
// All the memory except the last 32 bytes are allocated in blocks of minimum
// size (16 bytes), then odd blocks are released. After this startup,
// the worstcase occurs when a block of 32 bytes is requested."
TEST(BenchmarkTests, TLSF_Test1A) {
  fprintf(s_benchmark_tests_log_file, "TLSF_Test1A");

  static constexpr uint32_t kBlockSize_bytes = 16;
  static constexpr uint32_t kNAllocationsNeed =
      (EXT_RAM_SIZE / kBlockSize_bytes) - 2;

  // Allocate all blocks except last 2
  uint32_t offsets[kNAllocationsNeed];
  for (uint32_t i = 0; i < kNAllocationsNeed; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
    CHECK_EQUAL(0, offsets[i] & EMALLOC_ERR_MASK);
  }
}

TEST(BenchmarkTests, TLSF_Test1B) {
  fprintf(s_benchmark_tests_log_file, "TLSF_Test1B");

  static constexpr uint32_t kBlockSize_bytes = 16;
  static constexpr uint32_t kNAllocationsNeed =
      (EXT_RAM_SIZE / kBlockSize_bytes) - 2;

  // Allocate all blocks except last 2
  uint32_t offsets[kNAllocationsNeed];
  for (uint32_t i = 0; i < kNAllocationsNeed; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
    CHECK_EQUAL(0, offsets[i] & EMALLOC_ERR_MASK);
  }

  emalloc_reset_statistics();

  uint32_t last_offset = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes * 2);
  CHECK_EQUAL(0, last_offset & EMALLOC_ERR_MASK);
}

// "Test-2 malloc/free worst case for Douglas Lea's
// malloc. The memory pool is 256 KB and the same request
// sequence that in Test-1, but requesting blocks of 512 bytes.
// And the last block has a size of 530 bytes."
TEST(BenchmarkTests, TLSF_Test2A) {
  fprintf(s_benchmark_tests_log_file, "TLSF_Test2A");

  static constexpr uint32_t kBlockSize_bytes = 512;
  static constexpr uint32_t kNAllocationsNeed =
      (EXT_RAM_SIZE / kBlockSize_bytes) - 2;

  // Allocate all blocks except last 2
  uint32_t offsets[kNAllocationsNeed];
  for (uint32_t i = 0; i < kNAllocationsNeed; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
    CHECK_EQUAL(0, offsets[i] & EMALLOC_ERR_MASK);
  }
}

TEST(BenchmarkTests, TLSF_Test2B) {
  fprintf(s_benchmark_tests_log_file, "TLSF_Test2B");

  static constexpr uint32_t kBlockSize_bytes = 512;
  static constexpr uint32_t kNAllocationsNeed =
      (EXT_RAM_SIZE / kBlockSize_bytes) - 2;

  // Allocate all blocks except last 2
  uint32_t offsets[kNAllocationsNeed];
  for (uint32_t i = 0; i < kNAllocationsNeed; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
    CHECK_EQUAL(0, offsets[i] & EMALLOC_ERR_MASK);
  }

  emalloc_reset_statistics();

  uint32_t last_offset = emalloc_alloc(&emalloc_ctx, 530);
  CHECK_EQUAL(0, last_offset & EMALLOC_ERR_MASK);
}

// "Test-3 malloc/free worst case for Binary Buddy.
// The memory pool is 2 MB. No blocks were allocated initially.
// The worst case occurs when a 16 bytes block is requested.""
TEST(BenchmarkTests, TLSF_Test3) {
  fprintf(s_benchmark_tests_log_file, "TLSF_Test3");

  uint32_t last_offset = emalloc_alloc(&emalloc_ctx, 16);
  CHECK_EQUAL(0, last_offset & EMALLOC_ERR_MASK);
}

// "Test-4 malloc worst case for TLSF. The memory pool is 2 MB.
// No blocks are allocated initially. Then a 40-byte block is requested.""
TEST(BenchmarkTests, TLSF_Test4) {
  fprintf(s_benchmark_tests_log_file, "TLSF_Test4");

  uint32_t last_offset = emalloc_alloc(&emalloc_ctx, 40);
  CHECK_EQUAL(0, last_offset & EMALLOC_ERR_MASK);
}

// "Test-5 free worst case for TLSF. The memory pool is 1 MB.
// Three 512-byte blocks are allocated,
// then the first and third blocks are released. The time to release the second
// block is the worst-case scenario."
TEST(BenchmarkTests, TLSF_Test5A) {
  fprintf(s_benchmark_tests_log_file, "TLSF_Test5A");

  uint32_t block1 = emalloc_alloc(&emalloc_ctx, 512);
  CHECK_EQUAL(0, block1 & EMALLOC_ERR_MASK);

  uint32_t block2 = emalloc_alloc(&emalloc_ctx, 512);
  CHECK_EQUAL(0, block2 & EMALLOC_ERR_MASK);

  uint32_t block3 = emalloc_alloc(&emalloc_ctx, 512);
  CHECK_EQUAL(0, block3 & EMALLOC_ERR_MASK);

  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, block1));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, block3));
}

TEST(BenchmarkTests, TLSF_Test5B) {
  fprintf(s_benchmark_tests_log_file, "TLSF_Test5B");

  uint32_t block1 = emalloc_alloc(&emalloc_ctx, 512);
  CHECK_EQUAL(0, block1 & EMALLOC_ERR_MASK);

  uint32_t block2 = emalloc_alloc(&emalloc_ctx, 512);
  CHECK_EQUAL(0, block2 & EMALLOC_ERR_MASK);

  uint32_t block3 = emalloc_alloc(&emalloc_ctx, 512);
  CHECK_EQUAL(0, block3 & EMALLOC_ERR_MASK);

  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, block1));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, block3));

  emalloc_reset_statistics();

  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, block2));
}

TEST(BenchmarkTests, TLSF_Test5C) {
  fprintf(s_benchmark_tests_log_file, "TLSF_Test5C");

  static constexpr uint32_t kBlockSize_bytes = 512;
  static constexpr uint32_t kNAllocationsNeed =
      ((EXT_RAM_SIZE / kBlockSize_bytes) / 2) - 1;

  // Alloc first block group
  uint32_t first_block[kNAllocationsNeed];
  for (uint32_t i = 0; i < kNAllocationsNeed; i++) {
    first_block[i] = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
    CHECK_EQUAL(0, first_block[i] & EMALLOC_ERR_MASK);
  }

  // Alloc middle
  uint32_t middle = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
  CHECK_EQUAL(0, middle & EMALLOC_ERR_MASK);

  // Alloc second block group
  uint32_t second_block[kNAllocationsNeed];
  for (uint32_t i = 0; i < kNAllocationsNeed; i++) {
    second_block[i] = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
    CHECK_EQUAL(0, second_block[i] & EMALLOC_ERR_MASK);
  }

  // Free first block group
  for (uint32_t i = 0; i < kNAllocationsNeed; i++) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, first_block[i]));
  }

  // Free second block group
  for (uint32_t i = 0; i < kNAllocationsNeed; i++) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, second_block[i]));
  }

  emalloc_reset_statistics();

  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, middle));
}
