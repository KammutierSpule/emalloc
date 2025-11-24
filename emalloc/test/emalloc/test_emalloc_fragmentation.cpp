// /////////////////////////////////////////////////////////////////////////////
/// @file test_emalloc_fragmentation.cpp
/// @brief Unit tests for emalloc_alloc and emalloc_free
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
#include "helper_log.hpp"

// clang-format off
// NOLINTBEGIN
TEST_GROUP(MemoryFragmentation){
  static constexpr uint32_t EXT_RAM_SIZE = 4096;
  static constexpr uint32_t MAX_NODES = EXT_RAM_SIZE / 16;

  uint64_t nodes_poll[MAX_NODES];
  uint8_t external_ram[EXT_RAM_SIZE];

  sEMALLOC_ctx emalloc_ctx;

  void setup() {
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

  void teardown() {}
};
// NOLINTEND
// clang-format on

TEST(MemoryFragmentation, CheckerboadPattern) {
  // Allocate 16 blocks of 256 bytes each (4KB total)
  uint32_t offsets[16];
  for (int i = 0; i < 16; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, 256);
    CHECK(offsets[i] != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
  }

  // Free every other block (checkerboard pattern)
  for (int i = 0; i < 16; i += 2) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[i]));
  }

  // Now we have 8 free blocks of 256 bytes each
  // Try to allocate 512 bytes - should fail due to fragmentation
  uint32_t large = emalloc_alloc(&emalloc_ctx, 512);
  CHECK_EQUAL(EMALLOC_ERR_NO_EXTERNAL_MEMORY, large);

  // But should be able to allocate 256 bytes in freed spaces
  uint32_t small = emalloc_alloc(&emalloc_ctx, 256);
  CHECK(small != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
}

TEST(MemoryFragmentation, SwissCheesePattern) {
  // Allocate many small blocks
  uint32_t offsets[20];
  for (int i = 0; i < 20; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, 192);
    CHECK(offsets[i] != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
  }

  // Free random blocks to create "swiss cheese" holes
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[1]));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[5]));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[7]));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[11]));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[15]));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[18]));

  // Cannot allocate large contiguous block
  uint32_t large = emalloc_alloc(&emalloc_ctx, 1000);
  CHECK_EQUAL(EMALLOC_ERR_NO_EXTERNAL_MEMORY, large);

  // Can allocate small blocks in holes
  uint32_t small1 = emalloc_alloc(&emalloc_ctx, 100);
  uint32_t small2 = emalloc_alloc(&emalloc_ctx, 100);
  CHECK(small1 != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
  CHECK(small2 != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
}

TEST(MemoryFragmentation, ExternalFragmentationWorstCase) {
  // Allocate alternating large and small blocks
  uint32_t large[5];
  uint32_t small[4];

  for (int i = 0; i < 4; i++) {
    large[i] = emalloc_alloc(&emalloc_ctx, 760);
    CHECK(large[i] != EMALLOC_ERR_NO_EXTERNAL_MEMORY);

    small[i] = emalloc_alloc(&emalloc_ctx, 50);
    CHECK(small[i] != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
  }

  large[4] = emalloc_alloc(&emalloc_ctx, 760);

  // Free all small blocks
  for (int i = 0; i < 4; i++) {
    uint32_t free_ret = emalloc_free(&emalloc_ctx, small[i]);
    CHECK_EQUAL(EMALLOC_OK, free_ret);
  }

  // Now we have 4 x 50-byte holes
  // Cannot allocate anything larger than 50 bytes
  uint32_t medium = emalloc_alloc(&emalloc_ctx, 100);
  CHECK_EQUAL(EMALLOC_ERR_NO_EXTERNAL_MEMORY, medium);

  // Can allocate 50 bytes or less
  uint32_t tiny = emalloc_alloc(&emalloc_ctx, 50);
  CHECK(tiny != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
}

TEST(MemoryFragmentation, CoalescingReducesFragmentation) {
  // Allocate 10 blocks
  uint32_t offsets[10];
  for (int i = 0; i < 10; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, 400);
    CHECK(offsets[i] != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
  }

  // Free blocks 0,1,2 (adjacent) and 7,8,9 (adjacent)
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[0]));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[1]));

  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[2]));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[7]));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[8]));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[9]));

  // After coalescing, should have two 1200-byte blocks
  uint32_t large1 = emalloc_alloc(&emalloc_ctx, 1200);
  uint32_t large2 = emalloc_alloc(&emalloc_ctx, 1200);

  CHECK(large1 != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
  CHECK(large2 != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
}

TEST(MemoryFragmentation, ReverseOrderCoalescing) {
  uint32_t offsets[5];
  for (int i = 0; i < 5; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, 500);
    CHECK(offsets[i] != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
  }

  // Free in reverse order
  for (int i = 4; i >= 0; i--) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[i]));
  }

  // Should coalesce into one large block
  uint32_t full = emalloc_alloc(&emalloc_ctx, 2500);
  CHECK(full != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
}

TEST(MemoryFragmentation, RandomOrderCoalescing) {
  uint32_t offsets[6];
  for (int i = 0; i < 6; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, 300);
  }

  // Free in random order: 2, 4, 3, 1, 5, 0
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[2]));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[4]));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[3]));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[1]));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[5]));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[0]));

  // All should coalesce
  uint32_t full = emalloc_alloc(&emalloc_ctx, 1800);
  CHECK(full != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
}

TEST(MemoryFragmentation, BoundaryFragmentation) {
  // Allocate at start and end, leaving middle free
  uint32_t start = emalloc_alloc(&emalloc_ctx, 1000);
  uint32_t dummy[10];
  for (int i = 0; i < 10; i++) {
    dummy[i] = emalloc_alloc(&emalloc_ctx, 100);
    CHECK(dummy[i] != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
  }
  uint32_t end = emalloc_alloc(&emalloc_ctx, 1000);

  // Free middle blocks
  for (int i = 0; i < 10; i++) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, dummy[i]));
  }

  debug_header(&emalloc_ctx);
  debug_all_nodes_poll(&emalloc_ctx);

  // Should be able to allocate in coalesced middle
  uint32_t middle = emalloc_alloc(&emalloc_ctx, 1000);
  CHECK(middle != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
  CHECK(middle > start);
  CHECK(middle < end);
}

TEST(MemoryFragmentation, TinyBlockFragmentation) {
  // Allocate many tiny blocks
  static constexpr uint32_t block_size = 32;
  static constexpr uint32_t kNBlocks = EXT_RAM_SIZE / block_size;
  uint32_t offsets[kNBlocks];
  for (int i = 0; i < kNBlocks; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, block_size);
    CHECK_EQUAL(0, offsets[i] & EMALLOC_ERR_MASK);
  }

  // Free every 3rd block
  for (int i = 0; i < kNBlocks; i += 3) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[i]));
  }

  // Highly fragmented - cannot allocate medium block
  uint32_t medium = emalloc_alloc(&emalloc_ctx, block_size * 4);
  CHECK_EQUAL(EMALLOC_ERR_NO_EXTERNAL_MEMORY, medium);
}

TEST(MemoryFragmentation, GradualFragmentation) {
  // Allocate blocks of increasing size
  uint32_t offsets[10];
  for (int i = 0; i < 10; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, 100 + (i * 50));
    CHECK_EQUAL(0, offsets[i] & EMALLOC_ERR_MASK);
  }

  // Free odd-indexed blocks
  for (int i = 1; i < 10; i += 2) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[i]));
  }

  // Gaps are of different sizes: 150, 250, 350, 450, 550 bytes
  // Should be able to allocate into each gap
  uint32_t fit1 = emalloc_alloc(&emalloc_ctx, 150);
  uint32_t fit2 = emalloc_alloc(&emalloc_ctx, 250);
  uint32_t fit3 = emalloc_alloc(&emalloc_ctx, 350);

  CHECK(fit1 != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
  CHECK(fit2 != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
  CHECK(fit3 != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
}

TEST(MemoryFragmentation, MemoryExhaustionDueToFragmentation) {
  // Allocate pattern that wastes space
  uint32_t large[8];
  uint32_t small[7];

  for (int i = 0; i < 7; i++) {
    large[i] = emalloc_alloc(&emalloc_ctx, 448);
    CHECK_EQUAL(0, large[i] & EMALLOC_ERR_MASK);

    small[i] = emalloc_alloc(&emalloc_ctx, 50);
    CHECK_EQUAL(0, small[i] & EMALLOC_ERR_MASK);
  }

  large[7] = emalloc_alloc(&emalloc_ctx, 448);
  CHECK_EQUAL(0, large[7] & EMALLOC_ERR_MASK);

  // Free all small blocks (350 bytes total)
  for (int i = 0; i < 7; i++) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, small[i]));
  }

  // Even though 350 bytes are free, cannot allocate 100 bytes
  // because it's split into 7 x 50-byte fragments
  uint32_t cannot_fit = emalloc_alloc(&emalloc_ctx, 100);
  CHECK_EQUAL(EMALLOC_ERR_NO_EXTERNAL_MEMORY, cannot_fit);

  CHECK_COMPARE(emalloc_ctx.external_memory_size_bytes -
                    // cppcheck-suppress syntaxError
                    emalloc_ctx.external_allocated_bytes,
                >, 100);
}

TEST(MemoryFragmentation, DefragmentationByCompleteReset) {
  // Create heavy fragmentation
  uint32_t offsets[20];
  for (int i = 0; i < 20; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, 192);
    CHECK_EQUAL(0, offsets[i] & EMALLOC_ERR_MASK);
  }

  // Free every other block
  for (int i = 0; i < 20; i += 2) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[i]));
  }

  // Cannot allocate large block
  uint32_t large1 = emalloc_alloc(&emalloc_ctx, 2000);
  CHECK_EQUAL(EMALLOC_ERR_NO_EXTERNAL_MEMORY, large1);

  // Free everything
  for (int i = 1; i < 20; i += 2) {
    CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[i]));
  }

  // Now can allocate large block after full coalescing
  uint32_t large2 = emalloc_alloc(&emalloc_ctx, 4000);
  CHECK(large2 != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
}

TEST(MemoryFragmentation, PartialCoalescingScenario) {
  uint32_t offsets[8];
  for (int i = 0; i < 8; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, 400);
    CHECK_EQUAL(0, offsets[i] & EMALLOC_ERR_MASK);
  }

  // Free blocks 1,2 and 5,6 (two separate groups)
  emalloc_free(&emalloc_ctx, offsets[1]);
  emalloc_free(&emalloc_ctx, offsets[2]);
  emalloc_free(&emalloc_ctx, offsets[5]);
  emalloc_free(&emalloc_ctx, offsets[6]);

  // Should have two 800-byte blocks
  uint32_t block1 = emalloc_alloc(&emalloc_ctx, 800);
  uint32_t block2 = emalloc_alloc(&emalloc_ctx, 800);

  CHECK_EQUAL(0, block1 & EMALLOC_ERR_MASK);
  CHECK_EQUAL(0, block2 & EMALLOC_ERR_MASK);

  // But cannot allocate 1600 bytes contiguously
  uint32_t too_large = emalloc_alloc(&emalloc_ctx, 1600);
  CHECK_EQUAL(EMALLOC_ERR_NO_EXTERNAL_MEMORY, too_large);
}

// EOF
// /////////////////////////////////////////////////////////////////////////////
