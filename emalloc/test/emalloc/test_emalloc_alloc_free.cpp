// /////////////////////////////////////////////////////////////////////////////
/// @file test_AllocFree.cpp
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
TEST_GROUP( AllocFree ){
  static const uint32_t MAX_NODES = 32;
  static const uint32_t EXT_RAM_SIZE = 4096;

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

TEST(AllocFree, InitializerCreatesOneFreeBLock) {
  uint32_t offset = emalloc_alloc(&emalloc_ctx, EXT_RAM_SIZE);
  CHECK_EQUAL(0, offset);
}

TEST(AllocFree, AllocateZeroReturnsError) {
  uint32_t offset = emalloc_alloc(&emalloc_ctx, 0);
  CHECK_EQUAL(EMALLOC_ERR_ZERO_REQUESTED, offset);
}

TEST(AllocFree, AllocateTooLargeReturnsError) {
  uint32_t offset = emalloc_alloc(&emalloc_ctx, EXT_RAM_SIZE + 1);
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER, offset);
}

TEST(AllocFree, FirstAllocationReturnsOffsetZero) {
  uint32_t offset = emalloc_alloc(&emalloc_ctx, 256);
  CHECK_EQUAL(0, offset);
}

TEST(AllocFree, SecondAllocationReturnsCorrectOffset) {
  uint32_t offset1 = emalloc_alloc(&emalloc_ctx, 256);
  uint32_t offset2 = emalloc_alloc(&emalloc_ctx, 512);

  CHECK_EQUAL(0, offset1);
  CHECK_EQUAL(256, offset2);
}

TEST(AllocFree, AllocateEntireMemory) {
  uint32_t offset = emalloc_alloc(&emalloc_ctx, EXT_RAM_SIZE);
  CHECK_EQUAL(0, offset);

  // Next allocation should fail
  uint32_t offset2 = emalloc_alloc(&emalloc_ctx, 1);
  CHECK_EQUAL(EMALLOC_ERR_NO_EXTERNAL_MEMORY, offset2);
}

TEST(AllocFree, FreeAndReallocate) {
  uint32_t offset1 = emalloc_alloc(&emalloc_ctx, 256);

  CHECK_EQUAL(2, emalloc_ctx.node_count);

  emalloc_free(&emalloc_ctx, offset1);

  CHECK_EQUAL(1, emalloc_ctx.node_count);

  uint32_t offset2 = emalloc_alloc(&emalloc_ctx, 256);
  CHECK_EQUAL(0, offset2);

  CHECK_EQUAL(2, emalloc_ctx.node_count);
}

TEST(AllocFree, OutOfMemoryAfterMultipleAllocations) {
  for (uint32_t i = 0; i < EXT_RAM_SIZE / 256; i++) {
    uint32_t offset = emalloc_alloc(&emalloc_ctx, 256);
    CHECK(offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
  }

  // Should fail now
  uint32_t offset = emalloc_alloc(&emalloc_ctx, 256);
  CHECK_EQUAL(EMALLOC_ERR_NO_EXTERNAL_MEMORY, offset);
}

TEST(AllocFree, FreeInvalidOffsetDoesNotCrash) {
  CHECK_EQUAL(EMALLOC_ERR_OFFSET_NOT_FOUND, emalloc_free(&emalloc_ctx, 9999));
  CHECK_EQUAL(EMALLOC_ERR_OFFSET_NOT_FOUND,
              emalloc_free(&emalloc_ctx, 0xFFFFFFFF));
}

TEST(AllocFree, FreeInvalidOffsetsAfterAlloc) {
  uint32_t offset = emalloc_alloc(&emalloc_ctx, 256);

  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_free(&emalloc_ctx, offset + 1));
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_free(&emalloc_ctx, offset + 2));
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_free(&emalloc_ctx, offset + 3));
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_free(&emalloc_ctx, offset + 4));
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_free(&emalloc_ctx, offset + 5));
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_free(&emalloc_ctx, offset + 6));
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_free(&emalloc_ctx, offset + 7));
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_free(&emalloc_ctx, offset + 8));
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_free(&emalloc_ctx, offset + 9));
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_free(&emalloc_ctx, offset + 10));
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_free(&emalloc_ctx, offset + 11));
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_free(&emalloc_ctx, offset + 12));
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_free(&emalloc_ctx, offset + 13));
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_free(&emalloc_ctx, offset + 14));
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_free(&emalloc_ctx, offset + 15));
  CHECK_EQUAL(EMALLOC_ERR_OFFSET_NOT_FOUND,
              emalloc_free(&emalloc_ctx, offset + 16));
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offset + 0));
  CHECK_EQUAL(EMALLOC_ERR_OFFSET_NOT_FOUND,
              emalloc_free(&emalloc_ctx, offset + 0));
}

TEST(AllocFree, DoubleFreeDoesNotCorruptMemory) {
  uint32_t offset = emalloc_alloc(&emalloc_ctx, 256);
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offset));
  CHECK_EQUAL(EMALLOC_ERR_OFFSET_NOT_FOUND, emalloc_free(&emalloc_ctx, offset));

  // Should still be able to allocate
  uint32_t offset2 = emalloc_alloc(&emalloc_ctx, 256);
  CHECK_EQUAL(0, offset2);
}

TEST(AllocFree, FragmentationScenario) {
  // Allocate alternating pattern
  uint32_t offsets[10];
  for (int i = 0; i < 10; i++) {
    offsets[i] = emalloc_alloc(&emalloc_ctx, 100);
  }

  // Free every other block
  for (int i = 0; i < 10; i += 2) {
    emalloc_free(&emalloc_ctx, offsets[i]);
  }

  // Should be able to allocate in freed blocks
  uint32_t offset = emalloc_alloc(&emalloc_ctx, 50);
  CHECK(offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
}

TEST(AllocFree, DataIntegrityAfterAllocations) {
  uint32_t offset1 = emalloc_alloc(&emalloc_ctx, 256);
  uint32_t offset2 = emalloc_alloc(&emalloc_ctx, 256);

  // Write data to first allocation
  uint32_t* data1 = reinterpret_cast<uint32_t*>(external_ram + offset1);
  data1[0] = 0xDEADBEEF;

  // Write data to second allocation
  uint32_t* data2 = reinterpret_cast<uint32_t*>(external_ram + offset2);
  data2[0] = 0xCAFEBABE;

  // Verify data integrity
  CHECK_EQUAL(0xDEADBEEF, data1[0]);
  CHECK_EQUAL(0xCAFEBABE, data2[0]);
}

TEST(AllocFree, MaxNodesLimit) {
  // Allocate many small blocks to hit node limit
  uint32_t last_valid = 0;
  for (uint32_t i = 0; i < MAX_NODES + 10; i++) {
    uint32_t offset = emalloc_alloc(&emalloc_ctx, 10);
    if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      last_valid = offset;
    }
  }

  // Should have allocated some blocks before hitting limit
  // cppcheck-suppress syntaxError
  CHECK_COMPARE(last_valid, >, 0);
}

TEST(AllocFree, AllocateExactFitBlock) {
  uint32_t offset1 = emalloc_alloc(&emalloc_ctx, 100);
  emalloc_free(&emalloc_ctx, offset1);

  // Allocate exact same size
  uint32_t offset2 = emalloc_alloc(&emalloc_ctx, 100);
  CHECK_EQUAL(0, offset2);
}

TEST(AllocFree, StressTest) {
  const int ITERATIONS = 100;
  uint32_t offsets[10];

  for (int iter = 0; iter < ITERATIONS; iter++) {
    // Allocate
    for (int i = 0; i < 10; i++) {
      offsets[i] = emalloc_alloc(&emalloc_ctx, 50 + (i * 10));
      CHECK(offsets[i] != EMALLOC_ERR_NO_EXTERNAL_MEMORY);
    }

    // Free half
    for (int i = 0; i < 5; i++) {
      if (offsets[i] != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[i]));
      }
    }

    // Free remaining
    for (int i = 5; i < 10; i++) {
      if (offsets[i] != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx, offsets[i]));
      }
    }
  }

  // Should be able to allocate entire memory after stress test
  uint32_t offset = emalloc_alloc(&emalloc_ctx, EXT_RAM_SIZE);

  CHECK_EQUAL(0, offset);
}

// EOF
// /////////////////////////////////////////////////////////////////////////////
