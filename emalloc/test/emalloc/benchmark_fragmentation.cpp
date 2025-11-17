// /////////////////////////////////////////////////////////////////////////////
/// @file benchmark_fragmentation.cpp
/// @brief
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

extern FILE* s_benchmark_frag_log_file;
#define EMALLOC_MIN_MEMORY_SIZE (16)

// NOLINTBEGIN
TEST_GROUP(BenchmarkFragmentation) {
  static const uint32_t EXT_RAM_SIZE = 2 * 1024 * 1024;
  static const uint32_t MAX_NODES = EXT_RAM_SIZE / EMALLOC_MIN_MEMORY_SIZE;

  uint64_t nodes_poll[MAX_NODES];
  uint8_t external_ram[EXT_RAM_SIZE];

  sEMALLOC_ctx emalloc_ctx;

  // Use fixed seed for reproducibility
  uint32_t random_seed = 42;

  uint32_t requested_size = 0;
  uint32_t remain_size = EXT_RAM_SIZE;

  void setup() {
    if (!s_benchmark_frag_log_file) {
      s_benchmark_frag_log_file = fopen("BenchmarkFragmentation.log", "w");

      fprintf(s_benchmark_frag_log_file, "EXT_RAM_SIZE:%u, MAX_NODES:%u\n",
              EXT_RAM_SIZE, MAX_NODES);

      fprintf(s_benchmark_frag_log_file, "%16s%10s%8s%12s%8s%11s%8s%8s\n",
              "TestName", "req_size", "%", "remain_size", "%", "ExtAlloc",
              "Wasted", "Nodes");
    }

    // Initialize buffers
    memset(nodes_poll, 0, sizeof(nodes_poll));
    memset(external_ram, 0, sizeof(external_ram));

    sEMALLOC_cfg emalloc_configuration;

    emalloc_configuration.nodes_poll = nodes_poll;
    emalloc_configuration.nodes_poll_length = MAX_NODES;
    emalloc_configuration.external_memory_size_bytes = EXT_RAM_SIZE;

    CHECK_EQUAL(EMALLOC_OK, emalloc_init(&emalloc_ctx, &emalloc_configuration));
  }

  void teardown() {
    fprintf(s_benchmark_frag_log_file, "%10u%8.3f%12u%8.3f%11u%8u%8u\n",
            requested_size,
            (requested_size * 100.0f) / static_cast<float>(EXT_RAM_SIZE),
            remain_size,
            (remain_size * 100.0f) / static_cast<float>(EXT_RAM_SIZE),
            emalloc_ctx.external_allocated_bytes,
            (emalloc_ctx.external_allocated_bytes - requested_size),
            emalloc_ctx.node_count);
  }
};
// NOLINTEND

TEST(BenchmarkFragmentation, RandomFullRemainRange) {
  fprintf(s_benchmark_frag_log_file, "%16s", "RandomFullRange");

  while (requested_size < EXT_RAM_SIZE) {
    const uint32_t req_size = rand_r(&random_seed) % remain_size;

    uint32_t offset = emalloc_alloc(&emalloc_ctx, req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    requested_size += req_size;
    remain_size -= req_size;
  }
}

TEST(BenchmarkFragmentation, Size128K) {
  fprintf(s_benchmark_frag_log_file, "%16s", "Size128K");

  while (requested_size < EXT_RAM_SIZE) {
    const uint32_t req_size = 128 * 1024;

    uint32_t offset = emalloc_alloc(&emalloc_ctx, req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    requested_size += req_size;
    remain_size -= req_size;
  }
}

TEST(BenchmarkFragmentation, Size100K) {
  fprintf(s_benchmark_frag_log_file, "%16s", "Size100K");

  while (requested_size < EXT_RAM_SIZE) {
    const uint32_t req_size = 100 * 1024;

    uint32_t offset = emalloc_alloc(&emalloc_ctx, req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    requested_size += req_size;
    remain_size -= req_size;
  }
}

TEST(BenchmarkFragmentation, Size1234) {
  fprintf(s_benchmark_frag_log_file, "%16s", "Size1234");

  while (requested_size < EXT_RAM_SIZE) {
    const uint32_t req_size = 1234;

    uint32_t offset = emalloc_alloc(&emalloc_ctx, req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    requested_size += req_size;
    remain_size -= req_size;
  }
}

// 1. Fill all memory with random requests.
TEST(BenchmarkFragmentation, AllocRnd2KNoFree) {
  static constexpr uint32_t max_random_allocation_size = 2048;
  fprintf(s_benchmark_frag_log_file, "%16s", "AllocRnd2KNoFree");

  std::vector<std::pair<uint32_t, uint32_t>> allocation_table;
  allocation_table.reserve(EXT_RAM_SIZE / EMALLOC_MIN_MEMORY_SIZE);

  while (true) {
    const uint32_t req_size =
        EMALLOC_MIN_MEMORY_SIZE +
        (rand_r(&random_seed) %
         (max_random_allocation_size - EMALLOC_MIN_MEMORY_SIZE));

    uint32_t offset = emalloc_alloc(&emalloc_ctx, req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    allocation_table.emplace_back(offset, req_size);

    requested_size += req_size;
    remain_size -= req_size;
  }
}

// 1. Fill all memory with random requests.
// 2. Free every 2nd
// 3. Fill all memory with random requests.
TEST(BenchmarkFragmentation, AllocRnd2KFree2nd) {
  static constexpr uint32_t max_random_allocation_size = 2048;
  fprintf(s_benchmark_frag_log_file, "%16s", "AllocRnd2KFree2nd");

  std::vector<std::pair<uint32_t, uint32_t>> allocation_table;
  allocation_table.reserve(EXT_RAM_SIZE / EMALLOC_MIN_MEMORY_SIZE);

  while (true) {
    const uint32_t req_size =
        EMALLOC_MIN_MEMORY_SIZE +
        (rand_r(&random_seed) %
         (max_random_allocation_size - EMALLOC_MIN_MEMORY_SIZE));

    uint32_t offset = emalloc_alloc(&emalloc_ctx, req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    allocation_table.emplace_back(offset, req_size);

    requested_size += req_size;
    remain_size -= req_size;
  }

  for (uint32_t i = 0; i < allocation_table.size(); i += 2) {
    CHECK_EQUAL(EMALLOC_OK,
                emalloc_free(&emalloc_ctx, allocation_table[i].first));

    const uint32_t free_size = allocation_table[i].second;
    requested_size -= free_size;
    remain_size += free_size;
  }

  while (true) {
    const uint32_t req_size =
        EMALLOC_MIN_MEMORY_SIZE +
        (rand_r(&random_seed) %
         (max_random_allocation_size - EMALLOC_MIN_MEMORY_SIZE));

    uint32_t offset = emalloc_alloc(&emalloc_ctx, req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    allocation_table.emplace_back(offset, req_size);

    requested_size += req_size;
    remain_size -= req_size;
  }
}

// 1. Fill all memory with random requests.
// 2. Free every 3nd
// 3. Fill all memory with random requests.
TEST(BenchmarkFragmentation, AllocRnd2KFree3nd) {
  static constexpr uint32_t max_random_allocation_size = 2048;
  fprintf(s_benchmark_frag_log_file, "%16s", "AllocRnd2KFree3nd");

  std::vector<std::pair<uint32_t, uint32_t>> allocation_table;
  allocation_table.reserve(EXT_RAM_SIZE / EMALLOC_MIN_MEMORY_SIZE);

  while (true) {
    const uint32_t req_size =
        EMALLOC_MIN_MEMORY_SIZE +
        (rand_r(&random_seed) %
         (max_random_allocation_size - EMALLOC_MIN_MEMORY_SIZE));

    uint32_t offset = emalloc_alloc(&emalloc_ctx, req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    allocation_table.emplace_back(offset, req_size);

    requested_size += req_size;
    remain_size -= req_size;
  }

  for (uint32_t i = 0; i < allocation_table.size(); i += 3) {
    CHECK_EQUAL(EMALLOC_OK,
                emalloc_free(&emalloc_ctx, allocation_table[i].first));

    const uint32_t free_size = allocation_table[i].second;
    requested_size -= free_size;
    remain_size += free_size;
  }

  while (true) {
    const uint32_t req_size =
        EMALLOC_MIN_MEMORY_SIZE +
        (rand_r(&random_seed) %
         (max_random_allocation_size - EMALLOC_MIN_MEMORY_SIZE));

    uint32_t offset = emalloc_alloc(&emalloc_ctx, req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    allocation_table.emplace_back(offset, req_size);

    requested_size += req_size;
    remain_size -= req_size;
  }
}

// 1. Fill all memory with random requests.
// 2. Free every 4nd
// 3. Fill all memory with random requests.
TEST(BenchmarkFragmentation, AllocRnd2KFree4nd) {
  static constexpr uint32_t max_random_allocation_size = 2048;
  fprintf(s_benchmark_frag_log_file, "%16s", "AllocRnd2KFree4nd");

  std::vector<std::pair<uint32_t, uint32_t>> allocation_table;
  allocation_table.reserve(EXT_RAM_SIZE / EMALLOC_MIN_MEMORY_SIZE);

  while (true) {
    const uint32_t req_size =
        EMALLOC_MIN_MEMORY_SIZE +
        (rand_r(&random_seed) %
         (max_random_allocation_size - EMALLOC_MIN_MEMORY_SIZE));

    uint32_t offset = emalloc_alloc(&emalloc_ctx, req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    allocation_table.emplace_back(offset, req_size);

    requested_size += req_size;
    remain_size -= req_size;
  }

  for (uint32_t i = 0; i < allocation_table.size(); i += 4) {
    CHECK_EQUAL(EMALLOC_OK,
                emalloc_free(&emalloc_ctx, allocation_table[i].first));

    const uint32_t free_size = allocation_table[i].second;
    requested_size -= free_size;
    remain_size += free_size;
  }

  while (true) {
    const uint32_t req_size =
        EMALLOC_MIN_MEMORY_SIZE +
        (rand_r(&random_seed) %
         (max_random_allocation_size - EMALLOC_MIN_MEMORY_SIZE));

    uint32_t offset = emalloc_alloc(&emalloc_ctx, req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    allocation_table.emplace_back(offset, req_size);

    requested_size += req_size;
    remain_size -= req_size;
  }
}

// 1. Fill all memory with random requests.
// 2. Free half of number of allocations, randomly
// 3. Fill all memory with random requests.
TEST(BenchmarkFragmentation, AllocRnd2KFreeHlfRnd) {
  static constexpr uint32_t max_random_allocation_size = 2048;
  fprintf(s_benchmark_frag_log_file, "%16s", "AllocRnd2KFreeHlfRnd");

  std::vector<std::pair<uint32_t, uint32_t>> allocation_table;
  allocation_table.reserve(EXT_RAM_SIZE / EMALLOC_MIN_MEMORY_SIZE);

  while (true) {
    const uint32_t req_size =
        EMALLOC_MIN_MEMORY_SIZE +
        (rand_r(&random_seed) %
         (max_random_allocation_size - EMALLOC_MIN_MEMORY_SIZE));

    uint32_t offset = emalloc_alloc(&emalloc_ctx, req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    allocation_table.emplace_back(offset, req_size);

    requested_size += req_size;
    remain_size -= req_size;
  }

  // Free half of allocations, randomly
  uint32_t n_free = 0;
  while (n_free < (allocation_table.size() / 2)) {
    const uint32_t idx_to_free =
        0 + (rand_r(&random_seed) % (allocation_table.size() - 1));

    if (allocation_table[idx_to_free].first != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      CHECK_EQUAL(
          EMALLOC_OK,
          emalloc_free(&emalloc_ctx, allocation_table[idx_to_free].first));

      allocation_table[idx_to_free].first = EMALLOC_ERR_NO_EXTERNAL_MEMORY;

      const uint32_t free_size = allocation_table[idx_to_free].second;
      requested_size -= free_size;
      remain_size += free_size;
      n_free++;
    }
  }

  // Try to alloc the maximum of random requests
  while (true) {
    const uint32_t req_size =
        EMALLOC_MIN_MEMORY_SIZE +
        (rand_r(&random_seed) %
         (max_random_allocation_size - EMALLOC_MIN_MEMORY_SIZE));

    uint32_t offset = emalloc_alloc(&emalloc_ctx, req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    allocation_table.emplace_back(offset, req_size);

    requested_size += req_size;
    remain_size -= req_size;
  }
}

// 1. Fill all memory with random requests.
// 2. Free half of number of allocations, randomly
// 3. Fill all memory with random requests.
TEST(BenchmarkFragmentation, AllocRnd1KFreeHlfRnd) {
  static constexpr uint32_t max_random_allocation_size = 1024;
  fprintf(s_benchmark_frag_log_file, "%16s", "AllocRnd1KFreeHlfRnd");

  std::vector<std::pair<uint32_t, uint32_t>> allocation_table;
  allocation_table.reserve(EXT_RAM_SIZE / EMALLOC_MIN_MEMORY_SIZE);

  while (true) {
    const uint32_t req_size =
        EMALLOC_MIN_MEMORY_SIZE +
        (rand_r(&random_seed) %
         (max_random_allocation_size - EMALLOC_MIN_MEMORY_SIZE));

    uint32_t offset = emalloc_alloc(&emalloc_ctx, req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    allocation_table.emplace_back(offset, req_size);

    requested_size += req_size;
    remain_size -= req_size;
  }

  // Free half of allocations, randomly
  uint32_t n_free = 0;
  while (n_free < (allocation_table.size() / 2)) {
    const uint32_t idx_to_free =
        0 + (rand_r(&random_seed) % (allocation_table.size() - 1));

    if (allocation_table[idx_to_free].first != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      CHECK_EQUAL(
          EMALLOC_OK,
          emalloc_free(&emalloc_ctx, allocation_table[idx_to_free].first));

      allocation_table[idx_to_free].first = EMALLOC_ERR_NO_EXTERNAL_MEMORY;

      const uint32_t free_size = allocation_table[idx_to_free].second;
      requested_size -= free_size;
      remain_size += free_size;
      n_free++;
    }
  }

  // Try to alloc the maximum of random requests
  while (true) {
    const uint32_t req_size =
        EMALLOC_MIN_MEMORY_SIZE +
        (rand_r(&random_seed) %
         (max_random_allocation_size - EMALLOC_MIN_MEMORY_SIZE));

    uint32_t offset = emalloc_alloc(&emalloc_ctx, req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    allocation_table.emplace_back(offset, req_size);

    requested_size += req_size;
    remain_size -= req_size;
  }
}
