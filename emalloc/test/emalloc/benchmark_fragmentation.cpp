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
#include <random>

static bool s_show_header = true;

// clang-format off
// NOLINTBEGIN
TEST_GROUP( BenchmarkFragmentation ){
  static const uint32_t EXT_RAM_SIZE = 32*1024*1024;
  static const uint32_t MAX_NODES = EXT_RAM_SIZE / 16;

  uint64_t nodes_poll[MAX_NODES];
  uint8_t external_ram[EXT_RAM_SIZE];

  sEMALLOC_ctx emalloc_ctx;

  std::random_device random_device;
  std::mt19937 *random_generator;

  uint32_t requested_size = 0;
  uint32_t remain_size = EXT_RAM_SIZE;

  void setup() {
    if (s_show_header) {
      s_show_header = false;
      fprintf( stdout,"%16s%16s%8s%16s%8s%16s%8s%8s\n",
        "TestName",
        "requested_size",
        "%",
        "remain_size",
        "%",
        "ExtAlloc",
        "Diff",
        "Nodes");
    }


    random_generator = new std::mt19937(random_device());

    // Initialize buffers
    memset(nodes_poll, 0, sizeof(nodes_poll));
    memset(external_ram, 0, sizeof(external_ram));

    sEMALLOC_cfg emalloc_configuration;

    emalloc_configuration.nodes_poll = nodes_poll;
    emalloc_configuration.nodes_poll_length = MAX_NODES;
    emalloc_configuration.external_memory_size_bytes = EXT_RAM_SIZE;

    CHECK_EQUAL(EMALLOC_OK,
      emalloc_init(&emalloc_ctx,
        &emalloc_configuration));
  }

  void teardown() {
    delete random_generator;

    fprintf( stdout,"%16u%8.3f%16u%8.3f%16u%8u%8u\n",
      requested_size,
      (requested_size * 100.0f) / static_cast<float>(EXT_RAM_SIZE),
      remain_size,
      (remain_size * 100.0f) / static_cast<float>(EXT_RAM_SIZE),
      emalloc_ctx.external_allocated_bytes,
      (emalloc_ctx.external_allocated_bytes - requested_size),
      emalloc_ctx.node_count);
  }
};

TEST(BenchmarkFragmentation, RandomFullRemainRange) {
  fprintf( stdout,"%16s", "RandomFullRange");

  while(requested_size < EXT_RAM_SIZE) {
    std::uniform_int_distribution<> dist(0, static_cast<uint32_t>(remain_size));
    const uint32_t req_size = dist(*random_generator);

    uint32_t offset = emalloc_alloc(&emalloc_ctx,req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    requested_size += req_size;
    remain_size -=  req_size;
  }
}

TEST(BenchmarkFragmentation, Size128K) {
  fprintf( stdout,"%16s", "Size128K");

  while(requested_size < EXT_RAM_SIZE) {
    const uint32_t req_size = 128 * 1024;

    uint32_t offset = emalloc_alloc(&emalloc_ctx,req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    requested_size += req_size;
    remain_size -=  req_size;
  }
}

TEST(BenchmarkFragmentation, Size100K) {
  fprintf( stdout,"%16s", "Size100K");

  while(requested_size < EXT_RAM_SIZE) {
    const uint32_t req_size = 100 * 1024;

    uint32_t offset = emalloc_alloc(&emalloc_ctx,req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    requested_size += req_size;
    remain_size -=  req_size;
  }
}

TEST(BenchmarkFragmentation, Size1234) {
  fprintf( stdout,"%16s", "Size1234");

  while(requested_size < EXT_RAM_SIZE) {
    const uint32_t req_size = 1234;

    uint32_t offset = emalloc_alloc(&emalloc_ctx,req_size);
    CHECK_FALSE(offset == EMALLOC_ERR_NO_MORE_FREE_NODES);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;
    }

    requested_size += req_size;
    remain_size -=  req_size;
  }
}
