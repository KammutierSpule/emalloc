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
#include <vector>

extern FILE* s_benchmark_tests_log_file;
#define EMALLOC_MIN_MEMORY_SIZE (16)

// NOLINTBEGIN
TEST_GROUP(BenchmarkTests) {
  static const uint32_t EXT_RAM_SIZE = 512 * 1024;
  // This allows the teorically maximum nodes to be allocated
  static const uint32_t MAX_NODES = EXT_RAM_SIZE / EMALLOC_MIN_MEMORY_SIZE;

  uint64_t nodes_poll[MAX_NODES];
  uint8_t external_ram[EXT_RAM_SIZE];

  sEMALLOC_ctx emalloc_ctx;

  // Use fixed seed for reproducibility
  uint32_t random_seed = 42;

  uint32_t requested_size = 0;
  uint32_t remain_size = EXT_RAM_SIZE;

  void setup() {
    if (!s_benchmark_tests_log_file) {
      s_benchmark_tests_log_file = fopen("BenchmarkTests.log", "w");

      fprintf(s_benchmark_tests_log_file, "EXT_RAM_SIZE:%u, MAX_NODES:%u\n",
              EXT_RAM_SIZE, MAX_NODES);

      fprintf(s_benchmark_tests_log_file, "alloc/free\n");
      fprintf(s_benchmark_tests_log_file,
              "TestName     n_ifs\tn_loops\trd_wr,\tn_ifs\tn_loops\trd_wr\n");
    }

    // Initialize buffers
    memset(nodes_poll, 0, sizeof(nodes_poll));
    memset(external_ram, 0, sizeof(external_ram));

    sEMALLOC_cfg emalloc_configuration;

    emalloc_configuration.nodes_poll = nodes_poll;
    emalloc_configuration.nodes_poll_length = MAX_NODES;
    emalloc_configuration.external_memory_size_bytes = EXT_RAM_SIZE;

    emalloc_reset_statistics();

    CHECK_EQUAL(EMALLOC_OK, emalloc_init(&emalloc_ctx, &emalloc_configuration));
  }

  void teardown() {
    sEMALLOC_statistics stats;
    emalloc_get_statistics(&stats);

    fprintf(
        s_benchmark_tests_log_file, "\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n",
        stats.alloc.n_calls ? (stats.alloc.n_ifs / stats.alloc.n_calls) : 0,
        stats.alloc.n_calls ? (stats.alloc.n_loops / stats.alloc.n_calls) : 0,
        stats.alloc.n_calls ? (stats.alloc.n_nodes_rd_wr / stats.alloc.n_calls)
                            : 0,
        stats.free.n_calls ? (stats.free.n_ifs / stats.free.n_calls) : 0,
        stats.free.n_calls ? (stats.free.n_loops / stats.free.n_calls) : 0,
        stats.free.n_calls ? (stats.free.n_nodes_rd_wr / stats.free.n_calls)
                           : 0);
  }
};
// NOLINTEND

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

// Get statistics for "The time to release the second block"
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

// Get statistics for "The time to release the second block"
// Same but with a larger first and second block
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

// Alloc all, get stats for last allocation and first free
TEST(BenchmarkTests, AllFreeFirst16) {
  fprintf(s_benchmark_tests_log_file, "AllFreeFirst16");

  static constexpr uint32_t kBlockSize_bytes = 16;
  static constexpr uint32_t kNAllocationsNeed =
      (EXT_RAM_SIZE / kBlockSize_bytes) - 1;

  // Alloc first block group
  uint32_t offsets[kNAllocationsNeed];
  for (uint32_t i = 0; i < kNAllocationsNeed; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
    CHECK_EQUAL(0, offsets[i] & EMALLOC_ERR_MASK);
  }

  emalloc_reset_statistics();

  // Alloc last
  uint32_t middle = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
  CHECK_EQUAL(0, middle & EMALLOC_ERR_MASK);

  // Free first
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[0]));
}

// Alloc all, get stats for last allocation and first free
TEST(BenchmarkTests, AllFreeFirst1025) {
  fprintf(s_benchmark_tests_log_file, "AllFreeFirst1025");

  static constexpr uint32_t kBlockSize_bytes = 1025;
  static constexpr uint32_t kNAllocationsNeed =
      (EXT_RAM_SIZE / (((kBlockSize_bytes / EMALLOC_MIN_MEMORY_SIZE) + 1) *
                       EMALLOC_MIN_MEMORY_SIZE)) -
      1;

  // Alloc first block group
  uint32_t offsets[kNAllocationsNeed];
  for (uint32_t i = 0; i < kNAllocationsNeed; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
    CHECK_EQUAL(0, offsets[i] & EMALLOC_ERR_MASK);
  }

  emalloc_reset_statistics();

  // Alloc last
  uint32_t middle = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
  CHECK_EQUAL(0, middle & EMALLOC_ERR_MASK);

  // Free first
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[0]));
}

// Alloc all, get stats for last free
TEST(BenchmarkTests, AllFreeLast16) {
  fprintf(s_benchmark_tests_log_file, "AllFreeLast16");

  static constexpr uint32_t kBlockSize_bytes = 16;
  static constexpr uint32_t kNAllocationsNeed =
      (EXT_RAM_SIZE / kBlockSize_bytes);

  // Alloc first block group
  uint32_t offsets[kNAllocationsNeed];
  for (uint32_t i = 0; i < kNAllocationsNeed; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
    CHECK_EQUAL(0, offsets[i] & EMALLOC_ERR_MASK);
  }

  emalloc_reset_statistics();

  // Free Last
  CHECK_EQUAL(EMALLOC_OK,
              emalloc_free(&emalloc_ctx, offsets[kNAllocationsNeed - 1]));
}

// Alloc all, get stats for last free
TEST(BenchmarkTests, AllFreeLast1025) {
  fprintf(s_benchmark_tests_log_file, "AllFreeLast1025");

  static constexpr uint32_t kBlockSize_bytes = 1025;
  static constexpr uint32_t kNAllocationsNeed =
      (EXT_RAM_SIZE / (((kBlockSize_bytes / EMALLOC_MIN_MEMORY_SIZE) + 1) *
                       EMALLOC_MIN_MEMORY_SIZE));

  // Alloc first block group
  uint32_t offsets[kNAllocationsNeed];
  for (uint32_t i = 0; i < kNAllocationsNeed; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
    CHECK_EQUAL(0, offsets[i] & EMALLOC_ERR_MASK);
  }

  emalloc_reset_statistics();

  // Free Last
  CHECK_EQUAL(EMALLOC_OK,
              emalloc_free(&emalloc_ctx, offsets[kNAllocationsNeed - 1]));
}

// Alloc all, get stats for middle free and middle alloc
TEST(BenchmarkTests, AllFreeMiddle16) {
  fprintf(s_benchmark_tests_log_file, "AllFreeMiddle16");

  static constexpr uint32_t kBlockSize_bytes = 16;
  static constexpr uint32_t kNAllocationsNeed =
      (EXT_RAM_SIZE / kBlockSize_bytes);

  // Alloc first block group
  uint32_t offsets[kNAllocationsNeed];
  for (uint32_t i = 0; i < kNAllocationsNeed; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
    CHECK_EQUAL(0, offsets[i] & EMALLOC_ERR_MASK);
  }

  emalloc_reset_statistics();

  // Free Last
  CHECK_EQUAL(EMALLOC_OK,
              emalloc_free(&emalloc_ctx, offsets[kNAllocationsNeed / 2]));

  uint32_t middle = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
  CHECK_EQUAL(0, middle & EMALLOC_ERR_MASK);
}

// Alloc all, get stats for middle free and middle alloc
TEST(BenchmarkTests, AllFreeMiddle1025) {
  fprintf(s_benchmark_tests_log_file, "AllFreeMiddle1025");

  static constexpr uint32_t kBlockSize_bytes = 1025;
  static constexpr uint32_t kNAllocationsNeed =
      (EXT_RAM_SIZE / (((kBlockSize_bytes / EMALLOC_MIN_MEMORY_SIZE) + 1) *
                       EMALLOC_MIN_MEMORY_SIZE));

  // Alloc first block group
  uint32_t offsets[kNAllocationsNeed];
  for (uint32_t i = 0; i < kNAllocationsNeed; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
    CHECK_EQUAL(0, offsets[i] & EMALLOC_ERR_MASK);
  }

  emalloc_reset_statistics();

  // Free Middle
  CHECK_EQUAL(EMALLOC_OK,
              emalloc_free(&emalloc_ctx, offsets[kNAllocationsNeed / 2]));

  uint32_t middle = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
  CHECK_EQUAL(0, middle & EMALLOC_ERR_MASK);
}

// Alloc and free all, stats from free starting on last
TEST(BenchmarkTests, FreeFromLast128) {
  fprintf(s_benchmark_tests_log_file, "FreeFromLast128");

  static constexpr uint32_t kBlockSize_bytes = 128;
  static constexpr uint32_t kNAllocationsNeed =
      (EXT_RAM_SIZE / kBlockSize_bytes);

  // Alloc first block group
  uint32_t offsets[kNAllocationsNeed];
  for (uint32_t i = 0; i < kNAllocationsNeed; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
    CHECK_EQUAL(0, offsets[i] & EMALLOC_ERR_MASK);
  }

  emalloc_reset_statistics();

  for (int32_t i = (kNAllocationsNeed - 1); i >= 0; i--) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[i]));
  }
}

// Alloc and free all, stats from free starting on last
TEST(BenchmarkTests, FreeFromLast1025) {
  fprintf(s_benchmark_tests_log_file, "FreeFromLast1025");

  static constexpr uint32_t kBlockSize_bytes = 1025;
  static constexpr uint32_t kNAllocationsNeed =
      (EXT_RAM_SIZE / (((kBlockSize_bytes / EMALLOC_MIN_MEMORY_SIZE) + 1) *
                       EMALLOC_MIN_MEMORY_SIZE));

  // Alloc first block group
  uint32_t offsets[kNAllocationsNeed];
  for (int32_t i = 0; i < kNAllocationsNeed; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, kBlockSize_bytes);
    CHECK_EQUAL(0, offsets[i] & EMALLOC_ERR_MASK);
  }

  emalloc_reset_statistics();

  for (int32_t i = (kNAllocationsNeed - 1); i >= 0; i--) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[i]));
  }
}

// Alloc 5, release 3, 8 + 16 bytes
TEST(BenchmarkTests, Alloc5Release3_16b) {
  fprintf(s_benchmark_tests_log_file, "Alloc5Release3_16b");

  static constexpr uint32_t kNAllocations = 5;

  std::vector<uint32_t> allocation_table;

  while (true) {
    // Allocate phase
    for (int i = 0; i < kNAllocations; i++) {
      const uint32_t size = 8 + (rand_r(&random_seed) % (16));
      const uint32_t offset = emalloc_alloc(&emalloc_ctx, size);

      if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        allocation_table.push_back(offset);
      } else {
        return;  // Until memory exaustion
      }
    }

    // Free phase - free random allocations
    for (int i = 0; i < 3; i++) {
      if (!allocation_table.empty()) {
        size_t idx = rand_r(&random_seed) % allocation_table.size();
        CHECK_EQUAL(EMALLOC_OK,
                    emalloc_free(&emalloc_ctx, allocation_table[idx]));

        allocation_table.erase(allocation_table.begin() + idx);
      }
    }
  }
}

// Alloc 5, release 3, 16 + 64 bytes
TEST(BenchmarkTests, Alloc5Release3_64b) {
  fprintf(s_benchmark_tests_log_file, "Alloc5Release3_64b");

  static constexpr uint32_t kNAllocations = 5;

  std::vector<uint32_t> allocation_table;

  while (true) {
    // Allocate phase
    for (int i = 0; i < kNAllocations; i++) {
      const uint32_t size = 16 + (rand_r(&random_seed) % (64));
      const uint32_t offset = emalloc_alloc(&emalloc_ctx, size);

      if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        allocation_table.push_back(offset);
      } else {
        return;  // Until memory exaustion
      }
    }

    // Free phase - free random allocations
    for (int i = 0; i < 3; i++) {
      if (!allocation_table.empty()) {
        size_t idx = rand_r(&random_seed) % allocation_table.size();
        CHECK_EQUAL(EMALLOC_OK,
                    emalloc_free(&emalloc_ctx, allocation_table[idx]));

        allocation_table.erase(allocation_table.begin() + idx);
      }
    }
  }
}

// Alloc 5, release 3, 16 + 128 bytes
TEST(BenchmarkTests, Alloc5Release3_128b) {
  fprintf(s_benchmark_tests_log_file, "Alloc5Release3_128b");

  static constexpr uint32_t kNAllocations = 5;

  std::vector<uint32_t> allocation_table;

  while (true) {
    // Allocate phase
    for (int i = 0; i < kNAllocations; i++) {
      const uint32_t size = 16 + (rand_r(&random_seed) % (128));
      const uint32_t offset = emalloc_alloc(&emalloc_ctx, size);

      if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        allocation_table.push_back(offset);
      } else {
        return;  // Until memory exaustion
      }
    }

    // Free phase - free random allocations
    for (int i = 0; i < 3; i++) {
      if (!allocation_table.empty()) {
        size_t idx = rand_r(&random_seed) % allocation_table.size();
        CHECK_EQUAL(EMALLOC_OK,
                    emalloc_free(&emalloc_ctx, allocation_table[idx]));

        allocation_table.erase(allocation_table.begin() + idx);
      }
    }
  }
}

// Alloc 5, release 3, 16 + 512 bytes
TEST(BenchmarkTests, Alloc5Release3_512b) {
  fprintf(s_benchmark_tests_log_file, "Alloc5Release3_512b");

  static constexpr uint32_t kNAllocations = 5;

  std::vector<uint32_t> allocation_table;

  while (true) {
    // Allocate phase
    for (int i = 0; i < kNAllocations; i++) {
      const uint32_t size = 16 + (rand_r(&random_seed) % (512));
      const uint32_t offset = emalloc_alloc(&emalloc_ctx, size);

      if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        allocation_table.push_back(offset);
      } else {
        return;  // Until memory exaustion
      }
    }

    // Free phase - free random allocations
    for (int i = 0; i < 3; i++) {
      if (!allocation_table.empty()) {
        size_t idx = rand_r(&random_seed) % allocation_table.size();
        CHECK_EQUAL(EMALLOC_OK,
                    emalloc_free(&emalloc_ctx, allocation_table[idx]));

        allocation_table.erase(allocation_table.begin() + idx);
      }
    }
  }
}

// Alloc 5, release 3, 16 + 1024 bytes
TEST(BenchmarkTests, Alloc5Release3_1024b) {
  fprintf(s_benchmark_tests_log_file, "Alloc5Release3_1024b");

  static constexpr uint32_t kNAllocations = 5;

  std::vector<uint32_t> allocation_table;

  while (true) {
    // Allocate phase
    for (int i = 0; i < kNAllocations; i++) {
      const uint32_t size = 16 + (rand_r(&random_seed) % (512));
      const uint32_t offset = emalloc_alloc(&emalloc_ctx, size);

      if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        allocation_table.push_back(offset);
      } else {
        return;  // Until memory exaustion
      }
    }

    // Free phase - free random allocations
    for (int i = 0; i < 3; i++) {
      if (!allocation_table.empty()) {
        size_t idx = rand_r(&random_seed) % allocation_table.size();
        CHECK_EQUAL(EMALLOC_OK,
                    emalloc_free(&emalloc_ctx, allocation_table[idx]));

        allocation_table.erase(allocation_table.begin() + idx);
      }
    }
  }
}

// Alloc 5, release 3, 16 + 2 Kbytes
TEST(BenchmarkTests, Alloc5Release3_2k) {
  fprintf(s_benchmark_tests_log_file, "Alloc5Release3_2k");

  static constexpr uint32_t kNAllocations = 5;

  std::vector<uint32_t> allocation_table;

  while (true) {
    // Allocate phase
    for (int i = 0; i < kNAllocations; i++) {
      const uint32_t size = 16 + (rand_r(&random_seed) % (2048));
      const uint32_t offset = emalloc_alloc(&emalloc_ctx, size);

      if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        allocation_table.push_back(offset);
      } else {
        return;  // Until memory exaustion
      }
    }

    // Free phase - free random allocations
    for (int i = 0; i < 3; i++) {
      if (!allocation_table.empty()) {
        size_t idx = rand_r(&random_seed) % allocation_table.size();
        CHECK_EQUAL(EMALLOC_OK,
                    emalloc_free(&emalloc_ctx, allocation_table[idx]));

        allocation_table.erase(allocation_table.begin() + idx);
      }
    }
  }
}

// Alloc 5, release 3, 1024 + 2 Kbytes
TEST(BenchmarkTests, Alloc5Release3_1to2k) {
  fprintf(s_benchmark_tests_log_file, "Alloc5Release3_1to2k");

  static constexpr uint32_t kNAllocations = 5;

  std::vector<uint32_t> allocation_table;

  while (true) {
    // Allocate phase
    for (int i = 0; i < kNAllocations; i++) {
      const uint32_t size = 1024 + (rand_r(&random_seed) % (2048));
      const uint32_t offset = emalloc_alloc(&emalloc_ctx, size);

      if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        allocation_table.push_back(offset);
      } else {
        return;  // Until memory exaustion
      }
    }

    // Free phase - free random allocations
    for (int i = 0; i < 3; i++) {
      if (!allocation_table.empty()) {
        size_t idx = rand_r(&random_seed) % allocation_table.size();
        CHECK_EQUAL(EMALLOC_OK,
                    emalloc_free(&emalloc_ctx, allocation_table[idx]));

        allocation_table.erase(allocation_table.begin() + idx);
      }
    }
  }
}

// Alloc 6, release 2, 8 + 16 bytes
TEST(BenchmarkTests, Alloc6Release2_16b) {
  fprintf(s_benchmark_tests_log_file, "Alloc6Release2_16b");

  static constexpr uint32_t kNAllocations = 6;

  std::vector<uint32_t> allocation_table;

  while (true) {
    // Allocate phase
    for (int i = 0; i < kNAllocations; i++) {
      const uint32_t size = 8 + (rand_r(&random_seed) % (16));
      const uint32_t offset = emalloc_alloc(&emalloc_ctx, size);

      if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        allocation_table.push_back(offset);
      } else {
        return;  // Until memory exaustion
      }
    }

    // Free phase - free random allocations
    for (int i = 0; i < 2; i++) {
      if (!allocation_table.empty()) {
        size_t idx = rand_r(&random_seed) % allocation_table.size();
        CHECK_EQUAL(EMALLOC_OK,
                    emalloc_free(&emalloc_ctx, allocation_table[idx]));

        allocation_table.erase(allocation_table.begin() + idx);
      }
    }
  }
}

// Alloc 6, release 2, 16 + 64 bytes
TEST(BenchmarkTests, Alloc6Release2_64b) {
  fprintf(s_benchmark_tests_log_file, "Alloc6Release2_64b");

  static constexpr uint32_t kNAllocations = 6;

  std::vector<uint32_t> allocation_table;

  while (true) {
    // Allocate phase
    for (int i = 0; i < kNAllocations; i++) {
      const uint32_t size = 16 + (rand_r(&random_seed) % (64));
      const uint32_t offset = emalloc_alloc(&emalloc_ctx, size);

      if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        allocation_table.push_back(offset);
      } else {
        return;  // Until memory exaustion
      }
    }

    // Free phase - free random allocations
    for (int i = 0; i < 2; i++) {
      if (!allocation_table.empty()) {
        size_t idx = rand_r(&random_seed) % allocation_table.size();
        CHECK_EQUAL(EMALLOC_OK,
                    emalloc_free(&emalloc_ctx, allocation_table[idx]));

        allocation_table.erase(allocation_table.begin() + idx);
      }
    }
  }
}

// Alloc 6, release 2, 16 + 128 bytes
TEST(BenchmarkTests, Alloc6Release2_128b) {
  fprintf(s_benchmark_tests_log_file, "Alloc6Release2_128b");

  static constexpr uint32_t kNAllocations = 6;

  std::vector<uint32_t> allocation_table;

  while (true) {
    // Allocate phase
    for (int i = 0; i < kNAllocations; i++) {
      const uint32_t size = 16 + (rand_r(&random_seed) % (128));
      const uint32_t offset = emalloc_alloc(&emalloc_ctx, size);

      if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        allocation_table.push_back(offset);
      } else {
        return;  // Until memory exaustion
      }
    }

    // Free phase - free random allocations
    for (int i = 0; i < 2; i++) {
      if (!allocation_table.empty()) {
        size_t idx = rand_r(&random_seed) % allocation_table.size();
        CHECK_EQUAL(EMALLOC_OK,
                    emalloc_free(&emalloc_ctx, allocation_table[idx]));

        allocation_table.erase(allocation_table.begin() + idx);
      }
    }
  }
}

// Alloc 6, release 2, 16 + 512 bytes
TEST(BenchmarkTests, Alloc6Release2_512b) {
  fprintf(s_benchmark_tests_log_file, "Alloc6Release2_512b");

  static constexpr uint32_t kNAllocations = 6;

  std::vector<uint32_t> allocation_table;

  while (true) {
    // Allocate phase
    for (int i = 0; i < kNAllocations; i++) {
      const uint32_t size = 16 + (rand_r(&random_seed) % (512));
      const uint32_t offset = emalloc_alloc(&emalloc_ctx, size);

      if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        allocation_table.push_back(offset);
      } else {
        return;  // Until memory exaustion
      }
    }

    // Free phase - free random allocations
    for (int i = 0; i < 2; i++) {
      if (!allocation_table.empty()) {
        size_t idx = rand_r(&random_seed) % allocation_table.size();
        CHECK_EQUAL(EMALLOC_OK,
                    emalloc_free(&emalloc_ctx, allocation_table[idx]));

        allocation_table.erase(allocation_table.begin() + idx);
      }
    }
  }
}

// Alloc 6, release 2, 16 + 1024 bytes
TEST(BenchmarkTests, Alloc6Release2_1024b) {
  fprintf(s_benchmark_tests_log_file, "Alloc6Release2_1024b");

  static constexpr uint32_t kNAllocations = 6;

  std::vector<uint32_t> allocation_table;

  while (true) {
    // Allocate phase
    for (int i = 0; i < kNAllocations; i++) {
      const uint32_t size = 16 + (rand_r(&random_seed) % (512));
      const uint32_t offset = emalloc_alloc(&emalloc_ctx, size);

      if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        allocation_table.push_back(offset);
      } else {
        return;  // Until memory exaustion
      }
    }

    // Free phase - free random allocations
    for (int i = 0; i < 2; i++) {
      if (!allocation_table.empty()) {
        size_t idx = rand_r(&random_seed) % allocation_table.size();
        CHECK_EQUAL(EMALLOC_OK,
                    emalloc_free(&emalloc_ctx, allocation_table[idx]));

        allocation_table.erase(allocation_table.begin() + idx);
      }
    }
  }
}

// Alloc 6, release 2, 16 + 2 Kbytes
TEST(BenchmarkTests, Alloc6Release2_2k) {
  fprintf(s_benchmark_tests_log_file, "Alloc6Release2_2k");

  static constexpr uint32_t kNAllocations = 6;

  std::vector<uint32_t> allocation_table;

  while (true) {
    // Allocate phase
    for (int i = 0; i < kNAllocations; i++) {
      const uint32_t size = 16 + (rand_r(&random_seed) % (2048));
      const uint32_t offset = emalloc_alloc(&emalloc_ctx, size);

      if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        allocation_table.push_back(offset);
      } else {
        return;  // Until memory exaustion
      }
    }

    // Free phase - free random allocations
    for (int i = 0; i < 2; i++) {
      if (!allocation_table.empty()) {
        size_t idx = rand_r(&random_seed) % allocation_table.size();
        CHECK_EQUAL(EMALLOC_OK,
                    emalloc_free(&emalloc_ctx, allocation_table[idx]));

        allocation_table.erase(allocation_table.begin() + idx);
      }
    }
  }
}

// Alloc 6, release 2, 1024 + 2 Kbytes
TEST(BenchmarkTests, Alloc6Release2_1to2k) {
  fprintf(s_benchmark_tests_log_file, "Alloc6Release2_1to2k");

  static constexpr uint32_t kNAllocations = 6;

  std::vector<uint32_t> allocation_table;

  while (true) {
    // Allocate phase
    for (int i = 0; i < kNAllocations; i++) {
      const uint32_t size = 1024 + (rand_r(&random_seed) % (2048));
      const uint32_t offset = emalloc_alloc(&emalloc_ctx, size);

      if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        allocation_table.push_back(offset);
      } else {
        return;  // Until memory exaustion
      }
    }

    // Free phase - free random allocations
    for (int i = 0; i < 2; i++) {
      if (!allocation_table.empty()) {
        size_t idx = rand_r(&random_seed) % allocation_table.size();
        CHECK_EQUAL(EMALLOC_OK,
                    emalloc_free(&emalloc_ctx, allocation_table[idx]));

        allocation_table.erase(allocation_table.begin() + idx);
      }
    }
  }
}

TEST(BenchmarkTests, AllocAll16FreeOdd) {
  fprintf(s_benchmark_tests_log_file, "AllocAll16FreeOdd");

  static constexpr uint32_t kAllocationSize = 16;
  static constexpr uint32_t kNAllocations = EXT_RAM_SIZE / kAllocationSize;

  std::vector<uint32_t> allocation_table;

  allocation_table.resize(kNAllocations);

  // Allocate all
  for (uint32_t i = 0; i < kNAllocations; i++) {
    const uint32_t offset = emalloc_alloc(&emalloc_ctx, kAllocationSize);

    CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);
    allocation_table[i] = offset;
  }

  emalloc_reset_statistics();

  for (uint32_t i = 1; i < kNAllocations; i += 2) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, allocation_table[i]));
  }
}

TEST(BenchmarkTests, AllocAll16FreeOddReversed) {
  fprintf(s_benchmark_tests_log_file, "AllocAll16FreeOddReversed");

  static constexpr uint32_t kAllocationSize = 16;
  static constexpr uint32_t kNAllocations = EXT_RAM_SIZE / kAllocationSize;

  std::vector<uint32_t> allocation_table;

  allocation_table.resize(kNAllocations);

  // Allocate all
  for (uint32_t i = 0; i < kNAllocations; i++) {
    const uint32_t offset = emalloc_alloc(&emalloc_ctx, kAllocationSize);

    CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);
    allocation_table[i] = offset;
  }

  emalloc_reset_statistics();

  for (int32_t i = (kNAllocations - 1); i >= 1; i -= 2) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, allocation_table[i]));
  }
}

TEST(BenchmarkTests, AllocAll16FreeEven) {
  fprintf(s_benchmark_tests_log_file, "AllocAll16FreeEven");

  static constexpr uint32_t kAllocationSize = 16;
  static constexpr uint32_t kNAllocations = EXT_RAM_SIZE / kAllocationSize;

  std::vector<uint32_t> allocation_table;

  allocation_table.resize(kNAllocations);

  // Allocate all
  for (uint32_t i = 0; i < kNAllocations; i++) {
    const uint32_t offset = emalloc_alloc(&emalloc_ctx, kAllocationSize);

    CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);
    allocation_table[i] = offset;
  }

  emalloc_reset_statistics();

  for (uint32_t i = 0; i < kNAllocations; i += 2) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, allocation_table[i]));
  }
}

TEST(BenchmarkTests, AllocAll16FreeEvenReversed) {
  fprintf(s_benchmark_tests_log_file, "AllocAll16FreeEvenReversed");

  static constexpr uint32_t kAllocationSize = 16;
  static constexpr uint32_t kNAllocations = EXT_RAM_SIZE / kAllocationSize;

  std::vector<uint32_t> allocation_table;

  allocation_table.resize(kNAllocations);

  // Allocate all
  for (uint32_t i = 0; i < kNAllocations; i++) {
    const uint32_t offset = emalloc_alloc(&emalloc_ctx, kAllocationSize);

    CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);
    allocation_table[i] = offset;
  }

  emalloc_reset_statistics();

  for (int32_t i = ((kNAllocations - 1) - 1); i >= 0; i -= 2) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, allocation_table[i]));
  }
}

TEST(BenchmarkTests, AllocAll16FreeEvenOdd) {
  fprintf(s_benchmark_tests_log_file, "AllocAll16FreeEvenOdd");

  static constexpr uint32_t kAllocationSize = 16;
  static constexpr uint32_t kNAllocations = EXT_RAM_SIZE / kAllocationSize;

  std::vector<uint32_t> allocation_table;

  allocation_table.resize(kNAllocations);

  // Allocate all
  for (uint32_t i = 0; i < kNAllocations; i++) {
    const uint32_t offset = emalloc_alloc(&emalloc_ctx, kAllocationSize);

    CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);
    allocation_table[i] = offset;
  }

  for (uint32_t i = 0; i < kNAllocations; i += 2) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, allocation_table[i]));
  }

  emalloc_reset_statistics();

  for (uint32_t i = 1; i < kNAllocations; i += 2) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, allocation_table[i]));
  }
}

TEST(BenchmarkTests, AllocAll16FreeOddEven) {
  fprintf(s_benchmark_tests_log_file, "AllocAll16FreeOddEven");

  static constexpr uint32_t kAllocationSize = 16;
  static constexpr uint32_t kNAllocations = EXT_RAM_SIZE / kAllocationSize;

  std::vector<uint32_t> allocation_table;

  allocation_table.resize(kNAllocations);

  // Allocate all
  for (uint32_t i = 0; i < kNAllocations; i++) {
    const uint32_t offset = emalloc_alloc(&emalloc_ctx, kAllocationSize);

    CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);
    allocation_table[i] = offset;
  }

  for (uint32_t i = 1; i < kNAllocations; i += 2) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, allocation_table[i]));
  }

  emalloc_reset_statistics();

  for (uint32_t i = 0; i < kNAllocations; i += 2) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, allocation_table[i]));
  }
}

TEST(BenchmarkTests, AllocAll16FreeEvenOddReversed) {
  fprintf(s_benchmark_tests_log_file, "AllocAll16FreeEvenOddReversed");

  static constexpr uint32_t kAllocationSize = 16;
  static constexpr uint32_t kNAllocations = EXT_RAM_SIZE / kAllocationSize;

  std::vector<uint32_t> allocation_table;

  allocation_table.resize(kNAllocations);

  // Allocate all
  for (uint32_t i = 0; i < kNAllocations; i++) {
    const uint32_t offset = emalloc_alloc(&emalloc_ctx, kAllocationSize);

    CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);
    allocation_table[i] = offset;
  }

  for (uint32_t i = 0; i < kNAllocations; i += 2) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, allocation_table[i]));
  }

  emalloc_reset_statistics();

  for (int32_t i = (kNAllocations - 1); i >= 1; i -= 2) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, allocation_table[i]));
  }
}

TEST(BenchmarkTests, AllocAll16FreeOddEvenReversed) {
  fprintf(s_benchmark_tests_log_file, "AllocAll16FreeOddEvenReversed");

  static constexpr uint32_t kAllocationSize = 16;
  static constexpr uint32_t kNAllocations = EXT_RAM_SIZE / kAllocationSize;

  std::vector<uint32_t> allocation_table;

  allocation_table.resize(kNAllocations);

  // Allocate all
  for (uint32_t i = 0; i < kNAllocations; i++) {
    const uint32_t offset = emalloc_alloc(&emalloc_ctx, kAllocationSize);

    CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);
    allocation_table[i] = offset;
  }

  for (uint32_t i = 1; i < kNAllocations; i += 2) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, allocation_table[i]));
  }

  emalloc_reset_statistics();

  for (int32_t i = ((kNAllocations - 1) - 1); i >= 0; i -= 2) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, allocation_table[i]));
  }
}

// EOF
// /////////////////////////////////////////////////////////////////////////////
